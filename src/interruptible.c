#include "interruptible.h"
#include "error.h"
#include "main.h"

static const char* const MTINT_INTERRUPTIBLE_CLASS_NAME   = "mtint.interruptible";

static const char* const MTINT_COROUTINE_GUARD_CLASS_NAME = "mtint.guard.coroutine";

typedef struct MtInterruptible {
    lua_Integer        id;
    AtomicCounter      used;
    AtomicCounter      initialized;
    Lock               lock;
    lua_State*         L2;
    
    struct MtInterruptible**   prevInterruptiblePtr;
    struct MtInterruptible*    nextInterruptible;
    
} MtInterruptible;

typedef struct IntrUserData {
    MtInterruptible*  interruptible;
    bool              isMainGuard;
} IntrUserData;

typedef struct {
    int              count;
    MtInterruptible* firstInterruptible;
} InterruptibleBucket;

static AtomicCounter        interruptible_counter     = 0;
static lua_Integer          interruptible_buckets     = 0;
static int                  bucket_usage              = 0;
static InterruptibleBucket* interruptible_bucket_list = NULL;

inline static void toBuckets(MtInterruptible* intrp, lua_Integer n, InterruptibleBucket* list)
{
    InterruptibleBucket* bucket        = &(list[intrp->id % n]);
    MtInterruptible**    firstInterruptiblePtr = &bucket->firstInterruptible;
    if (*firstInterruptiblePtr) {
        (*firstInterruptiblePtr)->prevInterruptiblePtr = &intrp->nextInterruptible;
    }
    intrp->nextInterruptible    = *firstInterruptiblePtr;
    intrp->prevInterruptiblePtr =  firstInterruptiblePtr;
    *firstInterruptiblePtr  =  intrp;
    bucket->count += 1;
    if (bucket->count > bucket_usage) {
        bucket_usage = bucket->count;
    }
}

static void newBuckets(lua_Integer n, InterruptibleBucket* newList)
{
    bucket_usage = 0;
    if (interruptible_bucket_list) {
        lua_Integer i;
        for (i = 0; i < interruptible_buckets; ++i) {
            InterruptibleBucket* buck   = &(interruptible_bucket_list[i]);
            MtInterruptible*     intrp  = buck->firstInterruptible;
            while (intrp != NULL) {
                MtInterruptible* i2 = intrp->nextInterruptible;
                toBuckets(intrp, n, newList);
                intrp = i2;
            }
        }
        free(interruptible_bucket_list);
    }
    interruptible_buckets     = n;
    interruptible_bucket_list = newList;
}

static MtInterruptible* findInterruptibleWithId(lua_Integer interruptibleId)
{
    if (interruptible_bucket_list) {
        MtInterruptible* intrp = interruptible_bucket_list[interruptibleId % interruptible_buckets].firstInterruptible;
        while (intrp != NULL) {
            if (intrp->id == interruptibleId) {
                return intrp;
            }
            intrp = intrp->nextInterruptible;
        }
    }
    return NULL;
}


static const char* toLuaString(lua_State* L, IntrUserData* udata, MtInterruptible* intrp)
{
    if (intrp) {
        if (udata) {
            return lua_pushfstring(L, "%s: %p (id=%d)", MTINT_INTERRUPTIBLE_CLASS_NAME, udata, (int)intrp->id);
        } else {
            return lua_pushfstring(L, "%s(id=%d)", MTINT_INTERRUPTIBLE_CLASS_NAME, (int)intrp->id);
        }
    } else {
        return lua_pushfstring(L, "%s: invalid", MTINT_INTERRUPTIBLE_CLASS_NAME);
    }
}

const char* mtint_interruptible_tostring(lua_State* L, MtInterruptible* intrp)
{
    return toLuaString(L, NULL, intrp);
}

static const char* udataToLuaString(lua_State* L, IntrUserData* udata)
{
    if (udata) {
        return toLuaString(L, udata, udata->interruptible);
    } else {
        return lua_pushfstring(L, "%s: invalid", MTINT_INTERRUPTIBLE_CLASS_NAME);
    }
}

static int pushInterruptibleLookupTable(lua_State* L)
{
    lua_pushlightuserdata(L, (void*)MTINT_MODULE_NAME); /* unique void* key */
        lua_rawget(L, LUA_REGISTRYINDEX); /* -1, +1 */
        bool alreadyInitializedForThisLua = !lua_isnil(L, -1);

    if (!alreadyInitializedForThisLua) {
        lua_pop(L, 1);
        lua_newtable(L); /* lookup table */
            lua_pushlightuserdata(L, (void*)MTINT_MODULE_NAME); /* unique void* key */
                lua_pushvalue(L, -2); /* lookup table */
                    lua_newtable(L); /* metatable for lookup */
                        
                        lua_pushliteral(L, "k");              /* -0 +1 */
                        lua_setfield(L, -2, "__mode");        /* -1 +0 */
                        
                        lua_pushstring(L, MTINT_MODULE_NAME); /* -0 +1 */
                        lua_setfield(L, -2, "__metatable");   /* -1 +0 */
                        
                    lua_setmetatable(L, -2); /* -1 +0 */
            lua_rawset(L, LUA_REGISTRYINDEX); /* -2 +0 */
    }
    return 1;
}

static int GuardCoroutine_release(lua_State* L)
{
    if (lua_gettop(L) >= 1 && lua_type(L, -1) == LUA_TTABLE) {
        lua_rawgeti(L, -1, 1);       /* -0 +1 -> guard, udata */
            IntrUserData* udata = luaL_testudata(L, -1, MTINT_INTERRUPTIBLE_CLASS_NAME);
            if (udata && udata->interruptible) {
                async_lock_acquire(&udata->interruptible->lock);
                {
                    udata->interruptible->L2 = NULL;
                }
                async_lock_release(&udata->interruptible->lock);
            }
    }
    return 0;
}

static int pushCoroutineGuardMetaTable(lua_State* L)
{
    lua_pushlightuserdata(L, (void*)MTINT_COROUTINE_GUARD_CLASS_NAME); /* unique void* key */
        lua_rawget(L, LUA_REGISTRYINDEX); /* -1, +1 */
        bool alreadyInitializedForThisLua = !lua_isnil(L, -1);

    if (!alreadyInitializedForThisLua) {
        lua_pop(L, 1);
        lua_newtable(L); /* meta table */
            lua_pushlightuserdata(L, (void*)MTINT_COROUTINE_GUARD_CLASS_NAME); /* unique void* key */
                lua_pushvalue(L, -2); /* meta table */
                    
                    lua_pushcfunction(L, GuardCoroutine_release);        /* -0 +1 */
                    lua_setfield(L, -2, "__gc");                         /* -1 +0 */
                    
                    lua_pushstring(L, MTINT_COROUTINE_GUARD_CLASS_NAME); /* -0 +1 */
                    lua_setfield(L, -2, "__metatable");                  /* -1 +0 */
                    
            lua_rawset(L, LUA_REGISTRYINDEX); /* -2 +0 */
    }
    return 1;
}

static void setupInterruptibleGuard(lua_State* L, int intudata, int thread, bool isMainGuard)
{
    pushInterruptibleLookupTable(L);                    /* -> LT */
        lua_pushvalue(L, thread); /* lookup key            -> LT, thread */
        if (isMainGuard) {
            lua_pushvalue(L, intudata);                 /* -> LT, thread, guard==udata */
        } else {
            lua_newtable(L); /* value: interruptible guard -> LT, thread, guard  */
                lua_pushvalue(L, intudata);             /* -> LT, thread, guard, udata */
                lua_rawseti(L, -2, 1);    /* -1 +0         -> LT, thread, guard  */
                lua_pushvalue(L, thread);               /* -> LT, thread, guard, thread */
                lua_rawseti(L, -2, 2);    /* -1 +0         -> LT, thread, guard  */
                pushCoroutineGuardMetaTable(L);         /* -> LT, thread, guard, mt */
                lua_setmetatable(L, -2); /* -1 +0          -> LT, thread, guard  */
        }
        lua_rawset(L, -3);   /* -2 +0                      -> LT */
    lua_pop(L, 1);
}

/* pushes the udata on the stack and returns pointer, otherwise returns NULL 
 * and pushes nothing */
static IntrUserData* lookupIntrUserData(lua_State* L, int thread)
{
    IntrUserData* udata = NULL;
    
    pushInterruptibleLookupTable(L);      /*       -> LT  */
        lua_pushvalue(L, thread);         /* -0 +1 -> LT, thread */
            int tp = lua_rawget(L, -2);   /* -1 +1 -> LT, guard */
        if (tp == LUA_TTABLE) {
            lua_rawgeti(L, -1, 1);        /* -0 +1 -> LT, guard, udata */
                udata = luaL_testudata(L, -1, MTINT_INTERRUPTIBLE_CLASS_NAME);
                if (udata) { lua_remove(L, -2); 
                             lua_remove(L, -2); }
                else       { lua_pop(L, 3);     }
        } else if (tp == LUA_TUSERDATA) { /*       -> LT, guard==udata */
            udata = luaL_testudata(L, -1, MTINT_INTERRUPTIBLE_CLASS_NAME);
            if (udata) { lua_remove(L, -2); }
            else       { lua_pop(L, 2);     }
        } else {
            lua_pop(L, 2);
        }
            
    return udata;
}

static void setupInterruptibleMeta(lua_State* L);

static int pushInterruptibleMeta(lua_State* L)
{
    if (luaL_newmetatable(L, MTINT_INTERRUPTIBLE_CLASS_NAME)) {
        setupInterruptibleMeta(L);
    }
    return 1;
}

static int Mtint_id(lua_State* L)
{
    int arg       = 1;
    lua_State* L2 = NULL;
    int  thread   = 0;
    bool isMain   = false;
    
    if (!lua_isnoneornil(L, arg)) {
        luaL_checktype(L, arg, LUA_TTHREAD);
        L2     = lua_tothread(L, arg);
        isMain = lua_pushthread(L2); /* check if main */
                 lua_pop(L2, 1);
        thread = arg++;
    } else {
        isMain = lua_pushthread(L);
        L2     = L;
        thread = lua_gettop(L);
    }
#if LUA_VERSION_NUM == 501
    if (!isMain) {
        return mtint_ERROR_NOT_SUPPORTED(L, "coroutines cannot be interrupted in Lua 5.1");
    }
#endif

    IntrUserData*     udata = lookupIntrUserData(L, thread);
    MtInterruptible*  intrp;

    if (!udata || udata->interruptible == NULL || udata->interruptible->L2 != L2) 
    {
        IntrUserData* udata = lua_newuserdata(L, sizeof(IntrUserData));
        memset(udata, 0, sizeof(IntrUserData));
        pushInterruptibleMeta(L); /* -> udata, meta */
        lua_setmetatable(L, -2); /* -> udata */
        int udataIdx = lua_gettop(L);
        
        intrp = calloc(1, sizeof(MtInterruptible));
        if (!intrp) {
            return mtint_ERROR_OUT_OF_MEMORY(L);
        }
        intrp->id   = atomic_inc(&mtint_id_counter);
        intrp->used = 1;
        async_lock_init(&intrp->lock);
        udata->interruptible = intrp;
        udata->isMainGuard   = isMain;
        
        setupInterruptibleGuard(L, udataIdx, thread, isMain);

        async_mutex_lock(mtint_global_lock);
        {
            if (atomic_get(&interruptible_counter) + 1 > interruptible_buckets * 4 || bucket_usage > 30) {
                lua_Integer n = interruptible_buckets ? (2 * interruptible_buckets) : 64;
                InterruptibleBucket* newList = calloc(n, sizeof(InterruptibleBucket));
                if (newList) {
                    newBuckets(n, newList);
                } else if (!interruptible_buckets) {
                    async_mutex_unlock(mtint_global_lock);
                    return mtint_ERROR_OUT_OF_MEMORY(L);
                }
            }
            toBuckets(intrp, interruptible_buckets, interruptible_bucket_list);
            atomic_inc(&interruptible_counter);
            
            intrp->L2 = L2;
            atomic_set(&intrp->initialized, true);
        }
        async_mutex_unlock(mtint_global_lock);
    } else {
        intrp = udata->interruptible;
    }
    lua_pushinteger(L, intrp->id);
    return 1;
}

static int Mtint_interruptible(lua_State* L)
{
    lua_Integer interruptibleId = luaL_checkinteger(L, 1);

    IntrUserData* userData = lua_newuserdata(L, sizeof(IntrUserData)); /* -> udata */
    memset(userData, 0, sizeof(IntrUserData));
    pushInterruptibleMeta(L); /* -> udata, meta */
    lua_setmetatable(L, -2); /* -> udata */

    /* Lock */
    
    async_mutex_lock(mtint_global_lock);

    MtInterruptible* intrp = findInterruptibleWithId(interruptibleId);
    if (!intrp || !atomic_get(&intrp->initialized)) {
        async_mutex_unlock(mtint_global_lock);
        return mtint_ERROR_UNKNOWN_OBJECT_interruptible_id(L, interruptibleId);
    }
    userData->interruptible = intrp;
    atomic_inc(&intrp->used);
    
    async_mutex_unlock(mtint_global_lock);
    return 1;
}


static void MtInterruptible_free(MtInterruptible* intrp)
{
    bool wasInBucket = (intrp->prevInterruptiblePtr != NULL);

    if (wasInBucket) {
        *intrp->prevInterruptiblePtr = intrp->nextInterruptible;
    }
    if (intrp->nextInterruptible) {
        intrp->nextInterruptible->prevInterruptiblePtr = intrp->prevInterruptiblePtr;
    }
    
    async_lock_destruct(&intrp->lock);
    free(intrp);

    if (wasInBucket) {
        int c = atomic_dec(&interruptible_counter);
        if (c == 0) {
            if (interruptible_bucket_list)  {
                free(interruptible_bucket_list);
            }
            interruptible_buckets     = 0;
            interruptible_bucket_list = NULL;
            bucket_usage      = 0;
        }
        else if (c * 10 < interruptible_buckets) {
            lua_Integer n = 2 * c;
            if (n > 64) {
                InterruptibleBucket* newList = calloc(n, sizeof(InterruptibleBucket));
                if (newList) {
                    newBuckets(n, newList);
                }
            }
        }
    }
}

static int MtInterruptible_release(lua_State* L)
{
    IntrUserData*    udata = luaL_checkudata(L, 1, MTINT_INTERRUPTIBLE_CLASS_NAME);
    MtInterruptible* intrp = udata->interruptible;

    if (intrp) {

        if (atomic_dec(&intrp->used) == 0) {
            async_mutex_lock(mtint_global_lock);
            MtInterruptible_free(intrp);
            async_mutex_unlock(mtint_global_lock);
        }
        else if (udata->isMainGuard) {
            async_lock_acquire(&intrp->lock);
            intrp->L2 = NULL;
            async_lock_release(&intrp->lock);
        }
        udata->interruptible = NULL;
    }
    return 0;
}

static void interruptHook1(lua_State* L2, lua_Debug* ar) 
{
  (void)ar;  /* unused arg. */
  lua_sethook(L2, NULL, 0, 0);  /* reset hook */
  mtint_ERROR_INTERRUPTED(L2);
}

static void interruptHook2(lua_State* L2, lua_Debug* ar) 
{
  (void)ar;  /* unused arg. */
  mtint_ERROR_INTERRUPTED(L2);
}

static bool mtint_interrupt(MtInterruptible*  intrp, lua_Hook hook)
{
    bool signaled;
    async_lock_acquire(&intrp->lock);
    {
        if (intrp->L2) {
            if (hook) {
                lua_sethook(intrp->L2, hook, LUA_MASKCALL|LUA_MASKRET|LUA_MASKCOUNT, 1);
            } else {
                lua_sethook(intrp->L2, NULL, 0, 0);  /* reset hook */
            }
            signaled = true;
        } else {
            signaled = false;
        }
    }
    async_lock_release(&intrp->lock);
    return signaled;
}

static int MtInterruptible_interrupt(lua_State* L)
{   
    int arg = 1;
    IntrUserData*     udata = luaL_checkudata(L, arg++, MTINT_INTERRUPTIBLE_CLASS_NAME);
    MtInterruptible*  intrp = udata->interruptible;
    
    lua_Hook hook = interruptHook1;
    if (!lua_isnoneornil(L, arg)) {
        if (!lua_isboolean(L, arg)) {
            return luaL_argerror(L, arg, "boolean expected");
        }
        hook = lua_toboolean(L, arg) ? interruptHook2 : NULL;
    }
    lua_pushboolean(L, mtint_interrupt(intrp, hook));
    return 1;
}

static int Mtint_interrupt(lua_State* L)
{
    int              arg   = 1;
    IntrUserData*    udata = NULL;
    lua_Integer      intrId;
    
    if (!lua_isnoneornil(L, arg)) {
        if (lua_type(L, arg) == LUA_TUSERDATA) {
            udata = luaL_checkudata(L, arg++, MTINT_INTERRUPTIBLE_CLASS_NAME);
        }
        else {
            intrId = luaL_checkinteger(L, arg++);
        }
    } else {
        return luaL_argerror(L, 1, "interruptible object or id expected");
    }
    lua_Hook hook = interruptHook1;
    if (!lua_isnoneornil(L, arg)) {
        if (!lua_isboolean(L, arg)) {
            return luaL_argerror(L, arg, "boolean expected");
        }
        hook = lua_toboolean(L, arg++) ? interruptHook2 : NULL;
    }

    MtInterruptible* intrp;
    if (udata == NULL) {
        async_mutex_lock(mtint_global_lock);
        {
            intrp = findInterruptibleWithId(intrId);
            if (!intrp || !atomic_get(&intrp->initialized)) {
                async_mutex_unlock(mtint_global_lock);
                return mtint_ERROR_UNKNOWN_OBJECT_interruptible_id(L, intrId);
            }
            atomic_inc(&intrp->used);
        }
        async_mutex_unlock(mtint_global_lock);
    } else {
        intrp= udata->interruptible;
    }
    bool signaled = mtint_interrupt(intrp, hook);
    if (udata == NULL && atomic_dec(&intrp->used) == 0) {
        async_mutex_lock(mtint_global_lock);
        MtInterruptible_free(intrp);
        async_mutex_unlock(mtint_global_lock);
    }
    lua_pushboolean(L, signaled);
    return 1;
}

static int MtInterruptible_toString(lua_State* L)
{
    IntrUserData* udata = luaL_checkudata(L, 1, MTINT_INTERRUPTIBLE_CLASS_NAME);
    
    udataToLuaString(L, udata);
    return 1;
}


static int MtInterruptible_id(lua_State* L)
{
    int arg = 1;
    IntrUserData*     udata = luaL_checkudata(L, arg++, MTINT_INTERRUPTIBLE_CLASS_NAME);
    MtInterruptible*  intrp = udata->interruptible;
    lua_pushinteger(L, intrp->id);
    return 1;
}


static const luaL_Reg InterruptibleMethods[] = 
{
    { "id",         MtInterruptible_id         },
    { "interrupt",  MtInterruptible_interrupt  },
    { NULL,         NULL } /* sentinel */
};

static const luaL_Reg InterruptibleMetaMethods[] = 
{
    { "__tostring", MtInterruptible_toString },
    { "__gc",       MtInterruptible_release  },
    { NULL,         NULL } /* sentinel */
};

static const luaL_Reg ModuleFunctions[] = 
{
    { "id",                Mtint_id            },
    { "interrupt",         Mtint_interrupt     },
    { "interruptible",     Mtint_interruptible },
    { NULL,        NULL } /* sentinel */
};


static void setupInterruptibleMeta(lua_State* L)
{
    lua_pushstring(L, MTINT_INTERRUPTIBLE_CLASS_NAME);
    lua_setfield(L, -2, "__metatable");

    luaL_setfuncs(L, InterruptibleMetaMethods, 0);
    
    lua_newtable(L);  /* interruptibleClass */
        luaL_setfuncs(L, InterruptibleMethods, 0);
    lua_setfield (L, -2, "__index");
}

int mtint_interruptible_init_module(lua_State* L, int module)
{
    if (luaL_newmetatable(L, MTINT_INTERRUPTIBLE_CLASS_NAME)) {
        setupInterruptibleMeta(L);
    }
    lua_pop(L, 1);
    
    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);

    return 0;
}

