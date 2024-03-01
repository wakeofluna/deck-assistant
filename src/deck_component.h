#ifndef DECK_ASSISTANT_DECK_COMPONENT_H
#define DECK_ASSISTANT_DECK_COMPONENT_H

#include "lua_class.h"

class DeckComponent : public LuaClass<DeckComponent>
{
public:
	static char const* LUA_TYPENAME;

	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);

	int newindex(lua_State* L, lua_Integer key);
	int newindex(lua_State* L, std::string_view const& key);
};

#endif // DECK_ASSISTANT_DECK_COMPONENT_H
