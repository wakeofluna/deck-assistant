#ifndef DECK_ASSISTANT_DECK_CONNECTOR_FACTORY_H
#define DECK_ASSISTANT_DECK_CONNECTOR_FACTORY_H

#include "lua_class.h"

class DeckConnectorFactory : public LuaClass<DeckConnectorFactory>
{
public:
	static char const* LUA_TYPENAME;

	static void init_class_table(lua_State* L);
	int newindex(lua_State* L);
	int tostring(lua_State* L) const;

private:
	static int _lua_create_elgato_streamdeck(lua_State* L);
	static int _lua_create_websocket(lua_State* L);
	static int _lua_create_window(lua_State* L);
};

#endif // DECK_ASSISTANT_DECK_CONNECTOR_FACTORY_H
