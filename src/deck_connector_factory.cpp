#include "deck_connector_factory.h"
#include "connector_elgato_streamdeck.h"
#include "connector_websocket.h"
#include "connector_window.h"
#include "lua_helpers.h"

char const* DeckConnectorFactory::LUA_TYPENAME = "deck:ConnectorFactory";

void DeckConnectorFactory::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &_lua_create_elgato_streamdeck);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "ElgatoStreamDeck");
	lua_setfield(L, -2, "StreamDeck");

	lua_pushcfunction(L, &_lua_create_websocket);
	lua_setfield(L, -2, "Websocket");

	lua_pushcfunction(L, &_lua_create_window);
	lua_setfield(L, -2, "Window");
}

int DeckConnectorFactory::newindex(lua_State* L)
{
	from_stack(L, 1);
	luaL_checktype(L, 2, LUA_TSTRING);
	luaL_checktype(L, 3, LUA_TFUNCTION);
	LuaHelpers::newindex_store_in_instance_table(L);
	return 0;
}

int DeckConnectorFactory::tostring(lua_State* L) const
{
	lua_pushfstring(L, "%s: %p", LUA_TYPENAME, this);
	return 1;
}

int DeckConnectorFactory::_lua_create_elgato_streamdeck(lua_State* L)
{
	ConnectorElgatoStreamDeck::push_new(L);
	return 1;
}

int DeckConnectorFactory::_lua_create_websocket(lua_State* L)
{
	ConnectorWebsocket::push_new(L);
	return 1;
}

int DeckConnectorFactory::_lua_create_window(lua_State* L)
{
	ConnectorWindow::push_new(L);
	return 1;
}
