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

#ifndef DECK_ASSISTANT_CONNECTOR_SERVER_SOCKET_CLIENT_H
#define DECK_ASSISTANT_CONNECTOR_SERVER_SOCKET_CLIENT_H

#include "lua_class.h"
#include "util_socket.h"

class ConnectorServerSocketClient : public LuaClass<ConnectorServerSocketClient>
{
public:
	ConnectorServerSocketClient(util::Socket&& client_socket);
	~ConnectorServerSocketClient();

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	void finalize(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);

	std::string const& get_remote_host() const;
	int get_remote_port() const;

	int read_nonblock(void* data, int maxlen);
	bool is_connected() const;
	void close();

private:
	static int _lua_send(lua_State* L);
	static int _lua_close(lua_State* L);

private:
	util::Socket m_socket;
};

#endif // DECK_ASSISTANT_CONNECTOR_SERVER_SOCKET_CLIENT_H
