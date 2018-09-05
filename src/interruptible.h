#ifndef MTINT_STATE_H
#define MTINT_STATE_H

#include "util.h"

extern const char* const MTINT_INTERRUPTIBLE_CLASS_NAME;

int mtint_interruptible_init_module(lua_State* L, int module);

#endif /* MTINT_STATE_H */
