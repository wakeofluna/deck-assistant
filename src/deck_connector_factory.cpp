#include "deck_connector_factory.h"
#include "connector_elgato_streamdeck.h"
#include "connector_vnc.h"
#include "connector_websocket.h"
#include "connector_window.h"
#include "lua_helpers.h"

namespace
{

template <typename T>
int new_connector(lua_State* L)
{
	T::push_new(L);
	return 1;
}

int no_connector(lua_State* L)
{
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_error(L);
	return 0;
}

} // namespace

char const* DeckConnectorFactory::LUA_TYPENAME = "deck:ConnectorFactory";

void DeckConnectorFactory::init_class_table(lua_State* L)
{
	(void)no_connector;

	lua_pushcfunction(L, &new_connector<ConnectorElgatoStreamDeck>);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "ElgatoStreamDeck");
	lua_setfield(L, -2, "StreamDeck");

#ifdef HAVE_VNC
	lua_pushcfunction(L, &new_connector<ConnectorVnc>);
#else
	lua_pushliteral(L, "Vnc connector not available, recompile with libvncserver support");
	lua_pushcclosure(L, &no_connector, 1);
#endif
	lua_setfield(L, -2, "Vnc");

	lua_pushcfunction(L, &new_connector<ConnectorWebsocket>);
	lua_setfield(L, -2, "Websocket");

	lua_pushcfunction(L, &new_connector<ConnectorWindow>);
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
