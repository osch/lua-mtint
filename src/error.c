#include "error.h"

static const char* const MTINT_ERROR_CLASS_NAME = "mtint.error";

static const char* const MTINT_ERROR_NOT_SUPPORTED     = "not_supported";
static const char* const MTINT_ERROR_OBJECT_EXISTS     = "object_exists";
static const char* const MTINT_ERROR_OBJECT_CLOSED     = "object_closed";
static const char* const MTINT_ERROR_UNKNOWN_OBJECT    = "unknown_object";
static const char* const MTINT_ERROR_INTERRUPTED       = "interrupted";
static const char* const MTINT_ERROR_OUT_OF_MEMORY     = "out_of_memory";


static void pushErrorMessage(lua_State* L, const char* name, int details)
{
    if (details != 0) {
        lua_pushfstring(L, "%s.%s: %s", MTINT_ERROR_CLASS_NAME, 
                                        name, 
                                        lua_tostring(L, details));
    } else {
        lua_pushfstring(L, "%s.%s", MTINT_ERROR_CLASS_NAME, 
                                    name);
    }
}

/* error message details must be on top of stack */
static int throwErrorMessage(lua_State* L, const char* errorName)
{
    int messageDetails = lua_gettop(L);
    pushErrorMessage(L, errorName, messageDetails);
    lua_remove(L, messageDetails);
    return lua_error(L);
}

static int throwError(lua_State* L, const char* errorName)
{
    pushErrorMessage(L, errorName, 0);
    return lua_error(L);
}

int mtint_ERROR_OBJECT_EXISTS(lua_State* L, const char* objectString)
{
    lua_pushfstring(L, "%s", objectString);
    return throwErrorMessage(L, MTINT_ERROR_OBJECT_EXISTS);
}

int mtint_ERROR_OBJECT_CLOSED(lua_State* L, const char* objectString)
{
    lua_pushfstring(L, "%s", objectString);
    return throwErrorMessage(L, MTINT_ERROR_OBJECT_CLOSED);
}

int mtint_ERROR_UNKNOWN_OBJECT_interruptible_id(lua_State* L, lua_Integer id)
{
    lua_pushfstring(L, "interruptible id %d", (int)id);
    return throwErrorMessage(L, MTINT_ERROR_UNKNOWN_OBJECT);
}


int mtint_ERROR_INTERRUPTED(lua_State* L)
{
    return throwError(L, MTINT_ERROR_INTERRUPTED);
}

int mtint_ERROR_NOT_SUPPORTED(lua_State* L, const char* details)
{
    lua_pushstring(L, details);
    return throwErrorMessage(L, MTINT_ERROR_NOT_SUPPORTED);
}


int mtint_ERROR_OUT_OF_MEMORY(lua_State* L)
{
    return throwError(L, MTINT_ERROR_OUT_OF_MEMORY);
}

int mtint_ERROR_OUT_OF_MEMORY_bytes(lua_State* L, size_t bytes)
{
#if LUA_VERSION_NUM >= 503
    lua_pushfstring(L, "failed to allocate %I bytes", (lua_Integer)bytes);
#else
    lua_pushfstring(L, "failed to allocate %f bytes", (lua_Number)bytes);
#endif
    return throwErrorMessage(L, MTINT_ERROR_OUT_OF_MEMORY);
}



static void publishError(lua_State* L, int module, const char* errorName)
{
    lua_pushstring(L, errorName);
    lua_setfield(L, module, errorName);
}

int mtint_error_init_module(lua_State* L, int errorModule)
{
    publishError(L, errorModule, MTINT_ERROR_NOT_SUPPORTED);
    publishError(L, errorModule, MTINT_ERROR_OBJECT_EXISTS);
    publishError(L, errorModule, MTINT_ERROR_OBJECT_CLOSED);
    publishError(L, errorModule, MTINT_ERROR_UNKNOWN_OBJECT);
    publishError(L, errorModule, MTINT_ERROR_INTERRUPTED);
    publishError(L, errorModule, MTINT_ERROR_OUT_OF_MEMORY);
    
    return 0;
}

