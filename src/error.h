#ifndef MTINT_ERROR_H
#define MTINT_ERROR_H

#include "util.h"

typedef struct Error Error;

int mtint_ERROR_OBJECT_EXISTS(lua_State* L, const char* objectString);
int mtint_ERROR_OBJECT_CLOSED(lua_State* L, const char* objectString);

int mtint_ERROR_UNKNOWN_OBJECT_interruptible_id(lua_State* L, lua_Integer id);

int mtint_ERROR_INTERRUPTED(lua_State* L);

int mtint_ERROR_OUT_OF_MEMORY(lua_State* L);
int mtint_ERROR_OUT_OF_MEMORY_bytes(lua_State* L, size_t bytes);

int mtint_ERROR_NOT_SUPPORTED(lua_State* L, const char* details);

const char* mtint_error_details(lua_State* L, Error* e);
const char* mtint_error_name_and_details(lua_State* L, Error* e);
const char* mtint_error_traceback(lua_State* L, Error* e);

void mtint_error_init_meta(lua_State* L);

int mtint_error_init_module(lua_State* L, int module);


#endif /* MTINT_ERROR_H */
