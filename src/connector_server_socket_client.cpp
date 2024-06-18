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

#include "connector_server_socket_client.h"
#include "deck_logger.h"
#include "lua_helpers.h"

char const* ConnectorServerSocketClient::LUA_TYPENAME = "deck:ConnectorServerSocketClient";

ConnectorServerSocketClient::ConnectorServerSocketClient(util::Socket&& client_socket)
    : m_socket(std::move(client_socket))
{
}

ConnectorServerSocketClient::~ConnectorServerSocketClient()
{
}

void ConnectorServerSocketClient::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &_lua_send);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "write");
	lua_setfield(L, -2, "send");

	lua_pushcfunction(L, &_lua_close);
	lua_setfield(L, -2, "close");
}

void ConnectorServerSocketClient::finalize(lua_State* L)
{
	DeckLogger::log_message(L, DeckLogger::Level::Info, "ConnectorServerSocketClient for ", get_remote_host(), ':', get_remote_port(), " finalized");
}

int ConnectorServerSocketClient::index(lua_State* L, std::string_view const& key) const
{
	if (key == "connected")
	{
		lua_pushboolean(L, is_connected());
	}
	else if (key == "host" || key == "remote_host")
	{
		std::string const& rhost = get_remote_host();
		lua_pushlstring(L, rhost.data(), rhost.size());
	}
	else if (key == "port" || key == "remote_port")
	{
		int const rport = get_remote_port();
		lua_pushinteger(L, rport);
	}
	return lua_gettop(L) == 2 ? 0 : 1;
}

int ConnectorServerSocketClient::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "connected" || key == "host" || key == "remote_host" || key == "port" || key == "remote_port")
	{
		luaL_error(L, "key %s is readonly for %s", key.data(), LUA_TYPENAME);
	}
	else if (key.starts_with("on_"))
	{
		if (lua_type(L, 3) != LUA_TNIL)
			luaL_argcheck(L, (lua_type(L, 3) == LUA_TFUNCTION), 3, "event handlers must be functions");

		LuaHelpers::newindex_store_in_instance_table(L);
	}
	else
	{
		LuaHelpers::newindex_store_in_instance_table(L);
	}
	return 0;
}

std::string const& ConnectorServerSocketClient::get_remote_host() const
{
	return m_socket.get_remote_host();
}

int ConnectorServerSocketClient::get_remote_port() const
{
	return m_socket.get_remote_port();
}

int ConnectorServerSocketClient::read_nonblock(void* data, int maxlen)
{
	return m_socket.read_nonblock(data, maxlen);
}

bool ConnectorServerSocketClient::is_connected() const
{
	return m_socket.get_state() == util::Socket::State::Connected;
}

void ConnectorServerSocketClient::close()
{
	m_socket.close();
}

int ConnectorServerSocketClient::_lua_send(lua_State* L)
{
	ConnectorServerSocketClient* self = ConnectorServerSocketClient::from_stack(L, 1);
	std::string_view data             = LuaHelpers::check_arg_string(L, 2, true);

	if (!data.empty())
	{
		int const count = self->m_socket.write(data.data(), data.size());
		lua_pushinteger(L, count);
		return 1;
	}

	return 0;
}

int ConnectorServerSocketClient::_lua_close(lua_State* L)
{
	ConnectorServerSocketClient* self = ConnectorServerSocketClient::from_stack(L, 1);
	self->m_socket.close();
	return 0;
}
