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

#ifndef DECK_ASSISTANT_CONNECTOR_SERVER_SOCKET_H
#define DECK_ASSISTANT_CONNECTOR_SERVER_SOCKET_H

#include "connector_base.h"
#include "util_socket.h"
#include <vector>

class ConnectorServerSocket : public ConnectorBase<ConnectorServerSocket>
{
public:
	ConnectorServerSocket();
	~ConnectorServerSocket();

	void tick_inputs(lua_State* L, lua_Integer clock) override;
	void tick_outputs(lua_State* L) override;
	void shutdown(lua_State* L) override;

	void tick_server_input(lua_State* L, lua_Integer clock);
	void tick_clients_input(lua_State* L, lua_Integer clock);
	void tick_server_output(lua_State* L);
	void tick_clients_output(lua_State* L);

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);

private:
	static int _lua_reset_timer(lua_State* L);

private:
	enum class State : char
	{
		Disconnected,
		Binding,
		Listening,
	};

	std::shared_ptr<util::SocketSet> m_socketset;
	util::Socket m_socket;
	State m_server_state;
	unsigned short m_wanted_port;
	unsigned short m_active_port;
	bool m_enabled;
	lua_Integer m_listen_last_attempt;
	unsigned int m_num_clients;
	std::vector<char> m_read_buffer;
};

#endif // DECK_ASSISTANT_CONNECTOR_SERVER_SOCKET_H
