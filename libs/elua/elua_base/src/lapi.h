/*
** $Id: lapi.h,v 2.2.1.1 2007/12/27 13:02:25 roberto Exp $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lobject.h"


#ifdef __cplusplus
extern "C" {
#endif

LUAI_FUNC void luaA_pushobject (lua_State *L, const TValue *o);

#ifdef __cplusplus
}
#endif

#endif
