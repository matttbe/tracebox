#include "lua_global.h"

#include <ctime>
#include <cstring>

/***
 * @module Globals
 */

static void print_object(lua_State *L, int i, std::ostream& out)
{
	int t = lua_type(L, i);
	out << lua_typename(L, t) << ": ";
	switch (t) {
		case LUA_TSTRING:  /* strings */
			out << lua_tostring(L, i);
			break;

		case LUA_TBOOLEAN:  /* booleans */
			out << (lua_toboolean(L, i) ? "true" : "false");
			break;

		case LUA_TNUMBER:  /* numbers */
			out << lua_tonumber(L, i);
			break;

		case LUA_TUSERDATA:
			dynamic_cast<_ref_base*>((*(_ref_base**)
						(lua_touserdata(L, i))))->debug(out);
			break;

		default:  /* other values */
			break;
	}
}

void stackDump (lua_State *L, const char* f, size_t l, std::ostream& out) {
	out << "Globals:" << std::endl;
	lua_pushglobaltable(L);
	lua_pushnil(L);
	while (lua_next(L,-2) != 0) {
		if (lua_isstring(L, -2))
			out << "[" << luaL_checkstring(L, -2) << "] ";
		print_object(L, -1, out);
		lua_pop(L, 1);
		out << " / ";
	}
	lua_pop(L,1);
	out << "------------" << std::endl;

	int i;
	int top = lua_gettop(L);
	out << "Stack content (at " << f << "/" << l << "):" << std::endl;
	for (i = 1; i <= top; i++) {
		out << "[" << i << "] ";
		print_object(L, i, out);
		out << std::endl;
	}
	lua_pop(L, i);
	out << "===========" << std::endl;
}

/***
 * Suspend the current execution thread for a fixed amount of time
 * @function sleep
 * @tparam num the number of milliseconds during which the thread should sleep
 */
int l_sleep(lua_State *l)
{
	long ms = luaL_checkinteger(l, 1);
	struct timespec tp;
	tp.tv_nsec = 1000 * (ms % 1000);
	tp.tv_sec = ms / 1000;

	if (nanosleep(&tp, NULL))
		std::perror("sleep() failed");

	return 0;
}

/***
 * Return a string containing the content of the lua C stack
 * @function __dump_c_stack
 * @treturn string the content of the C stack
 */
int l_dump_stack(lua_State *l)
{
	std::ostringstream s;
	lua_Debug ar;
	lua_getstack(l, 1, &ar);
	lua_getinfo(l, "l", &ar);
	stackDump(l, "Called from Lua", ar.currentline, s);
	lua_pushstring(l, s.str().c_str());
	return 1;
}
