#include "main.h"
#include "error.h"
#include "interruptible.h"

#ifndef MTINT_VERSION
    #error MTINT_VERSION is not defined
#endif 

#define MTINT_STRINGIFY(x) #x
#define MTINT_TOSTRING(x) MTINT_STRINGIFY(x)
#define MTINT_VERSION_STRING MTINT_TOSTRING(MTINT_VERSION)

const char* const MTINT_MODULE_NAME = "mtint";

static Mutex         global_mutex;
static AtomicCounter initStage          = 0;
static bool          initialized        = false;
static int           stateCounter       = 0;

Mutex*        mtint_global_lock = NULL;
AtomicCounter mtint_id_counter  = 0;

/*static int internalError(lua_State* L, const char* text, int line) 
{
    return luaL_error(L, "Internal error: %s (%s:%d)", text, MTINT_MODULE_NAME, line);
}*/


static int handleClosingLuaState(lua_State* L)
{
    async_mutex_lock(mtint_global_lock);
    stateCounter -= 1;
    if (stateCounter == 0) {

    }
    async_mutex_unlock(mtint_global_lock);
    return 0;
}


static int Mtint_type(lua_State* L)
{
    luaL_checkany(L, 1);
    int tp = lua_type(L, 1);
    if (tp == LUA_TUSERDATA) {
        if (lua_getmetatable(L, 1)) {
            if (lua_getfield(L, -1, "__name") == LUA_TSTRING) {
                lua_pushvalue(L, -1);
                if (lua_gettable(L, LUA_REGISTRYINDEX) == LUA_TTABLE) {
                    if (lua_rawequal(L, -3, -1)) {
                        lua_pop(L, 1);
                        return 1;
                    }
                }
            }
        }
    }
    lua_pushstring(L, lua_typename(L, tp));
    return 1;
}

static const luaL_Reg ModuleFunctions[] = 
{
    { "type",    Mtint_type },      
    { NULL,      NULL          } /* sentinel */
};

DLL_PUBLIC int luaopen_mtint(lua_State* L)
{
    luaL_checkversion(L); /* does nothing if compiled for Lua 5.1 */

    /* ---------------------------------------- */

    if (atomic_get(&initStage) != 2) {
        if (atomic_set_if_equal(&initStage, 0, 1)) {
            async_mutex_init(&global_mutex);
            mtint_global_lock = &global_mutex;
            atomic_set(&initStage, 2);
        } 
        else {
            while (atomic_get(&initStage) != 2) {
                Mutex waitMutex;
                async_mutex_init(&waitMutex);
                async_mutex_lock(&waitMutex);
                async_mutex_wait_millis(&waitMutex, 1);
                async_mutex_destruct(&waitMutex);
            }
        }
    }
    /* ---------------------------------------- */

    async_mutex_lock(mtint_global_lock);
    {
        if (!initialized) {
            /* create initial id that could not accidently be mistaken with "normal" integers */
            const char* ptr = MTINT_MODULE_NAME;
            AtomicCounter c = 0;
            if (sizeof(AtomicCounter) - 1 >= 1) {
                int i;
                for (i = 0; i < 2 * sizeof(char*); ++i) {
                    c ^= ((int)(((char*)&ptr)[(i + 1) % sizeof(char*)]) & 0xff) << ((i % (sizeof(AtomicCounter) - 1))*8);
                }
                lua_Number t = mtint_current_time_seconds();
                for (i = 0; i < 2 * sizeof(lua_Number); ++i) {
                    c ^= ((int)(((char*)&t)[(i + 1) % sizeof(lua_Number)]) & 0xff) << ((i % (sizeof(AtomicCounter) - 1))*8);
                }
            }
            mtint_id_counter = c;
            initialized = true;
        }


        /* check if initialization has been done for this lua state */
        lua_pushlightuserdata(L, (void*)&initialized); /* unique void* key */
            lua_rawget(L, LUA_REGISTRYINDEX); 
            bool alreadyInitializedForThisLua = !lua_isnil(L, -1);
        lua_pop(L, 1);
        
        if (!alreadyInitializedForThisLua) 
        {
            stateCounter += 1;
            
            lua_pushlightuserdata(L, (void*)&initialized); /* unique void* key */
                lua_newuserdata(L, 1); /* sentinel for closing lua state */
                    lua_newtable(L); /* metatable for sentinel */
                        lua_pushstring(L, "__gc");
                            lua_pushcfunction(L, handleClosingLuaState);
                        lua_rawset(L, -3); /* metatable.__gc = handleClosingLuaState */
                    lua_setmetatable(L, -2); /* sets metatable for sentinal table */
            lua_rawset(L, LUA_REGISTRYINDEX); /* sets sentinel as value for unique void* in registry */
        }
    }
    async_mutex_unlock(mtint_global_lock);

    /* ---------------------------------------- */
    
    int n = lua_gettop(L);
    
    int module      = ++n; lua_newtable(L);
    int errorModule = ++n; lua_newtable(L);

    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);
    
    lua_pushliteral(L, MTINT_VERSION_STRING);
    lua_setfield(L, module, "_VERSION");
    
    {
    #if defined(MTINT_ASYNC_USE_WIN32)
        #define MTINT_INFO1 "WIN32"
    #elif defined(MTINT_ASYNC_USE_GNU)
        #define MTINT_INFO1 "GNU"
    #elif defined(MTINT_ASYNC_USE_STDATOMIC)
        #define MTINT_INFO1 "STD"
    #endif
    #if defined(MTINT_ASYNC_USE_WINTHREAD)
        #define MTINT_INFO2 "WIN32"
    #elif defined(MTINT_ASYNC_USE_PTHREAD)
        #define MTINT_INFO2 "PTHREAD"
    #elif defined(MTINT_ASYNC_USE_STDTHREAD)
        #define MTINT_INFO2 "STD"
    #endif
        lua_pushstring(L, "atomic:" MTINT_INFO1 ",thread:" MTINT_INFO2);
        lua_setfield(L, module, "_INFO");
    }
    
    lua_pushvalue(L, errorModule);
    lua_setfield(L, module, "error");

    lua_checkstack(L, LUA_MINSTACK);
    
    mtint_interruptible_init_module(L, module);
    mtint_error_init_module(L, errorModule);
    
    lua_settop(L, module);
    return 1;
}
