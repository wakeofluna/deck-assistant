#ifndef DECK_ASSISTANT_DECK_UTIL_H
#define DECK_ASSISTANT_DECK_UTIL_H

#include "lua_class.h"

class DeckUtil : public LuaClass<DeckUtil>
{
public:
	static char const* LUA_TYPENAME;
	static constexpr bool const LUA_IS_GLOBAL = true;

	static void init_class_table(lua_State* L);
	int newindex(lua_State* L);

private:
	static int _lua_from_base64(lua_State* L);
	static int _lua_to_base64(lua_State* L);
	static int _lua_from_hex(lua_State* L);
	static int _lua_to_hex(lua_State* L);
	static int _lua_sha1(lua_State* L);
	static int _lua_sha256(lua_State* L);
	static int _lua_random_bytes(lua_State* L);
};

#endif // DECK_ASSISTANT_DECK_UTIL_H
