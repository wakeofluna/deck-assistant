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

#include "connector_server_socket.h"
#include "connector_server_socket_client.h"
#include "deck_logger.h"
#include "lua_helpers.h"

char const* ConnectorServerSocket::LUA_TYPENAME = "deck:ConnectorServerSocket";

static constexpr int const MAX_CLIENTS = 10;

ConnectorServerSocket::ConnectorServerSocket()
    : m_socketset(util::SocketSet::create(MAX_CLIENTS + 1))
    , m_socket(m_socketset)
    , m_server_state(State::Disconnected)
    , m_wanted_port(0)
    , m_active_port(0)
    , m_enabled(true)
    , m_listen_last_attempt(-5000)
    , m_num_clients(0)
{
	m_read_buffer.assign(4096, 0);
}

ConnectorServerSocket::~ConnectorServerSocket()
{
}

void ConnectorServerSocket::tick_inputs(lua_State* L, lua_Integer clock)
{
	bool const have_activity = m_socketset->poll();

	tick_server_input(L, clock);

	if (have_activity)
		tick_clients_input(L, clock);
}

void ConnectorServerSocket::tick_outputs(lua_State* L)
{
	tick_server_output(L);
	tick_clients_output(L);
}

void ConnectorServerSocket::shutdown(lua_State* L)
{
	m_socket.close();
	m_server_state = State::Disconnected;
	m_active_port  = 0;

	if (m_num_clients)
	{
		LuaHelpers::push_instance_table(L, 1);
		lua_pushlightuserdata(L, this);
		lua_rawget(L, -2);

		for (unsigned int ref = 1; ref <= m_num_clients; ++ref)
		{
			lua_rawgeti(L, -1, ref);
			ConnectorServerSocketClient* client = ConnectorServerSocketClient::from_stack(L, -1, false);
			if (client)
			{
				client->close();
				lua_pop(L, 1);
			}
			lua_pushnil(L);
			lua_rawseti(L, -2, ref);
		}

		lua_pop(L, 2);
		m_num_clients = 0;
	}
}

void ConnectorServerSocket::tick_server_input(lua_State* L, lua_Integer clock)
{
	if (m_server_state == State::Disconnected)
	{
		if (!m_enabled || clock < m_listen_last_attempt + 5000)
			return;

		m_listen_last_attempt = clock;

		if (m_wanted_port == 0)
		{
			m_enabled = false;

			std::string_view message = "ServerSocket has not been assigned a port";
			DeckLogger::log_message(L, DeckLogger::Level::Error, message);
			LuaHelpers::emit_event(L, 1, "on_connect_failed", message);
			return;
		}

		m_active_port = m_wanted_port;
		m_socket.start_connect(std::string_view(), m_active_port);
		m_server_state = State::Binding;
	}

	if (m_server_state == State::Binding)
	{
		util::Socket::State const socket_state = m_socket.get_state();
		switch (socket_state)
		{
			case util::Socket::State::Disconnected:
				m_server_state = State::Disconnected;
				DeckLogger::log_message(L, DeckLogger::Level::Debug, "ServerSocket binding to port ", m_active_port, " failed: ", m_socket.get_last_error());
				m_active_port = 0;
				LuaHelpers::emit_event(L, 1, "on_connect_failed", m_socket.get_last_error());
				break;

			case util::Socket::State::Connecting:
				break;

			case util::Socket::State::Connected:
				m_server_state = State::Listening;
				DeckLogger::log_message(L, DeckLogger::Level::Debug, "ServerSocket bound to port ", m_active_port, ", now listening for connections");
				LuaHelpers::emit_event(L, 1, "on_connect");
				break;
		}
	}

	if (m_server_state == State::Listening)
	{
		std::optional<util::Socket> maybe_client = m_socket.accept_nonblock();
		if (maybe_client.has_value())
		{
			ConnectorServerSocketClient* client = ConnectorServerSocketClient::push_new(L, std::move(maybe_client).value());

			LuaHelpers::push_instance_table(L, 1);
			lua_pushlightuserdata(L, this);
			lua_gettable(L, -2);
			lua_pushvalue(L, -3);
			++m_num_clients;
			lua_rawseti(L, -2, m_num_clients);
			lua_pop(L, 2);

			DeckLogger::log_message(L, DeckLogger::Level::Debug, "ServerSocket on port ", m_active_port, " accepted client from ", client->get_remote_host(), ':', client->get_remote_port());
			LuaHelpers::emit_event(L, 1, "on_accept", LuaHelpers::StackValue(L, -1));
			lua_pop(L, 1);
		}

		if (m_socket.get_state() == util::Socket::State::Disconnected)
		{
			std::string message  = "ServerSocket on port ";
			message             += std::to_string(m_active_port);
			message             += " closed: ";
			message             += m_socket.get_last_error();

			m_server_state = State::Disconnected;
			m_active_port  = 0;
			DeckLogger::log_message(L, DeckLogger::Level::Debug, message);
			LuaHelpers::emit_event(L, 1, "on_disconnect", message);
			return;
		}
	}
}

void ConnectorServerSocket::tick_clients_input(lua_State* L, lua_Integer clock)
{
	LuaHelpers::push_instance_table(L, 1);
	lua_pushlightuserdata(L, this);
	lua_rawget(L, -2);

	unsigned int last_ok_ref = 0;
	for (unsigned int ref = 1; ref <= m_num_clients; ++ref)
	{
		lua_rawgeti(L, -1, ref);

		ConnectorServerSocketClient* client = ConnectorServerSocketClient::from_stack(L, -1, false);
		if (client)
		{
			if (client->is_connected())
			{
				int read_len = client->read_nonblock(m_read_buffer.data(), m_read_buffer.size());
				if (read_len > 0)
				{
					std::string_view read_buf(m_read_buffer.data(), read_len);
					LuaHelpers::emit_event(L, 1, "on_receive", LuaHelpers::StackValue(L, -1), read_buf);
				}
			}

			if (client->is_connected())
			{
				++last_ok_ref;
				if (ref > last_ok_ref)
				{
					lua_pushvalue(L, -1);
					lua_rawseti(L, -3, last_ok_ref);
				}
			}
			else
			{
				LuaHelpers::emit_event(L, 1, "on_close", LuaHelpers::StackValue(L, -1));
			}
		}

		lua_pop(L, 1);

		if (ref > last_ok_ref)
		{
			lua_pushnil(L);
			lua_rawseti(L, -2, ref);
		}
	}

	lua_pop(L, 2);
	m_num_clients = last_ok_ref;
}

void ConnectorServerSocket::tick_server_output(lua_State* L)
{
	if (m_server_state == State::Listening && !m_enabled)
	{
		std::string message  = "ServerSocket on port ";
		message             += std::to_string(m_active_port);
		message             += " disabled, closing port.";

		m_socket.close();
		m_server_state = State::Disconnected;
		m_active_port  = 0;
		DeckLogger::log_message(L, DeckLogger::Level::Debug, message);
		LuaHelpers::emit_event(L, 1, "on_disconnect", message);
	}
}

void ConnectorServerSocket::tick_clients_output(lua_State* L)
{
}

void ConnectorServerSocket::init_class_table(lua_State* L)
{
	Super::init_class_table(L);
}

void ConnectorServerSocket::init_instance_table(lua_State* L)
{
	lua_pushlightuserdata(L, this);
	lua_createtable(L, MAX_CLIENTS, 0);
	lua_settable(L, -3);

	LuaHelpers::create_callback_warning(L, "on_connect");
	LuaHelpers::create_callback_warning(L, "on_connect_failed");
	LuaHelpers::create_callback_warning(L, "on_disconnect");

	LuaHelpers::create_callback_warning(L, "on_accept");
	LuaHelpers::create_callback_warning(L, "on_receive");
	LuaHelpers::create_callback_warning(L, "on_close");
}

int ConnectorServerSocket::index(lua_State* L, std::string_view const& key) const
{
	if (key == "enabled")
	{
		lua_pushboolean(L, m_enabled);
	}
	else if (key == "port")
	{
		if (m_active_port != 0)
			lua_pushinteger(L, m_active_port);
		else
			lua_pushinteger(L, m_wanted_port);
	}
	return lua_gettop(L) == 2 ? 0 : 1;
}

int ConnectorServerSocket::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "enabled")
	{
		luaL_checktype(L, 3, LUA_TBOOLEAN);
		m_enabled = lua_toboolean(L, 3);
	}
	else if (key == "port")
	{
		int value = LuaHelpers::check_arg_int(L, 3);
		luaL_argcheck(L, (value > 0 && value < 65536), 3, "invalid value for port (out of range)");

		if (value != m_wanted_port)
		{
			if (m_server_state != State::Disconnected)
				DeckLogger::log_message(L, DeckLogger::Level::Warning, "ServerSocket already active on port ", m_active_port, ", active port may not change immediately");

			m_wanted_port          = value;
			m_listen_last_attempt -= 5000;
		}
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
