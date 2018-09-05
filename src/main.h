#ifndef MTINT_MAIN_H
#define MTINT_MAIN_H

#include "util.h"

extern const char* const MTINT_MODULE_NAME;
extern Mutex*            mtint_global_lock;
extern AtomicCounter     mtint_id_counter;

DLL_PUBLIC int luaopen_mtint(lua_State* L);

#endif /* MTINT_MAIN_H */
