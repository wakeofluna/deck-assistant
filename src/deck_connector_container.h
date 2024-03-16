#ifndef DECK_ASSISTANT_DECK_CONNECTOR_CONTAINER_H
#define DECK_ASSISTANT_DECK_CONNECTOR_CONTAINER_H

#include "lua_class.h"
#include <functional>

class DeckConnector;

class DeckConnectorContainer : public LuaClass<DeckConnectorContainer>
{
public:
	static char const* LUA_TYPENAME;

	void for_each(lua_State* L, std::function<void(lua_State* L, DeckConnector*)> const& visitor) const;

	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int newindex(lua_State* L);

private:
	static int _lua_all(lua_State* L);
	static int _lua_create_elgato_streamdeck(lua_State* L);
};

#endif // DECK_ASSISTANT_DECK_CONNECTOR_CONTAINER_H
