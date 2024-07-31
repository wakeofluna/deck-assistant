/*
 * DeckAssistant - Creating control panels using scripts.
 * Copyright (C) 2024  Esther Dalhuisen (Wake of Luna)
 *
 * DeckAssistant is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DeckAssistant is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "deck_connector_factory.h"
#include "connector_elgato_streamdeck.h"
#include "connector_http.h"
#include "connector_server_socket.h"
#include "connector_vnc.h"
#include "connector_websocket.h"
#include "connector_window.h"
#include "deck_module.h"
#include "lua_helpers.h"
#include <cassert>

namespace
{

template <typename T>
int new_connector(lua_State* L)
{
	T::push_new(L);
	return 1;
}

template <typename T>
int new_socket_connector(lua_State* L)
{
	DeckModule* deck = DeckModule::push_global_instance(L);
	assert(deck && "No DeckModule available when creating Connector");
	std::shared_ptr<util::SocketSet> sset = deck->get_socketset();
	lua_pop(L, 1);

	T::push_new(L, sset);
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
	(void)&no_connector;

	lua_pushcfunction(L, &new_connector<ConnectorElgatoStreamDeck>);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "ElgatoStreamDeck");
	lua_setfield(L, -2, "StreamDeck");

	lua_pushcfunction(L, &new_socket_connector<ConnectorHttp>);
	lua_setfield(L, -2, "Http");

	lua_pushcfunction(L, &new_socket_connector<ConnectorServerSocket>);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "ServerSocket");
	lua_setfield(L, -2, "Server");

#ifdef HAVE_VNC
	lua_pushcfunction(L, &new_connector<ConnectorVnc>);
#else
	lua_pushliteral(L, "Vnc connector not available, recompile with libvncserver support");
	lua_pushcclosure(L, &no_connector, 1);
#endif
	lua_setfield(L, -2, "Vnc");

	lua_pushcfunction(L, &new_socket_connector<ConnectorWebsocket>);
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
