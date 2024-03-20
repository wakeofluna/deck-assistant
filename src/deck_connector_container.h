#ifndef DECK_ASSISTANT_DECK_CONNECTOR_CONTAINER_H
#define DECK_ASSISTANT_DECK_CONNECTOR_CONTAINER_H

#include "lua_class.h"

class DeckConnectorContainer : public LuaClass<DeckConnectorContainer>
{
public:
	static void for_each(lua_State* L, char const* function_name, int nargs);

	static char const* LUA_TYPENAME;
	void init_instance_table(lua_State* L);
	int newindex(lua_State* L);
	int tostring(lua_State* L) const;
};

#endif // DECK_ASSISTANT_DECK_CONNECTOR_CONTAINER_H
