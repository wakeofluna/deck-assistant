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

#ifndef DECK_ASSISTANT_CONNECTOR_HTTP_H
#define DECK_ASSISTANT_CONNECTOR_HTTP_H

#include "connector_base.h"
#include "util_blob.h"
#include "util_socket.h"
#include "util_url.h"
#include <queue>
#include <utility>
#include <vector>

class ConnectorHttp : public ConnectorBase<ConnectorHttp>
{
public:
	ConnectorHttp(std::shared_ptr<util::SocketSet> const& socketset);
	~ConnectorHttp();

	void tick_inputs(lua_State* L, lua_Integer clock) override;
	void tick_outputs(lua_State* L, lua_Integer clock) override;
	void shutdown(lua_State* L) override;

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);

private:
	static int _lua_get(lua_State* L);
	static int _lua_post(lua_State* L);
	static int _lua_set_header(lua_State* L);
	static int _lua_clear_header(lua_State* L);

private:
	struct Request
	{
		util::BlobBuffer payload;
		int promise;
	};

	using HeadersVector = std::vector<std::pair<std::string, std::string>>;

	util::BlobBuffer compose_payload(lua_State* L, std::string_view const& path, std::string_view const& method, int headers_idx, std::string_view const& mimetype, std::string_view const& body);
	int queue_request(lua_State* L, util::BlobBuffer&& payload);

	util::Socket m_socket;
	util::URL m_base_url;
	HeadersVector m_default_headers;
	std::queue<Request> m_queue;
	util::Blob m_response;
	lua_Integer m_request_timeout;
	lua_Integer m_request_started_at;
	lua_Integer m_next_connect_attempt;
	int m_request_counter;
	int m_connect_attempts;
	bool m_enabled;
	bool m_insecure;
};

#endif // DECK_ASSISTANT_CONNECTOR_HTTP_H
