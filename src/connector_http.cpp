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

#include "connector_http.h"
#include "deck_logger.h"
#include "deck_promise_list.h"
#include "lua_helpers.h"
#include "util_text.h"
#include "util_url.h"
#include <cassert>
#include <charconv>
#include <string>

using namespace std::literals::string_view_literals;

namespace
{

constexpr std::string_view g_content_type     = "Content-Type";
constexpr std::string_view g_header_separator = ": ";
constexpr std::string_view g_header_newline   = "\r\n";

DeckPromiseList* push_promise_list(lua_State* L)
{
	LuaHelpers::push_instance_table(L, 1);
	lua_pushlightuserdata(L, lua_touserdata(L, 1));
	lua_rawget(L, -2);
	lua_replace(L, -2);
	return DeckPromiseList::from_stack(L, -1);
}

util::BlobBuffer compose_payload(lua_State* L, util::URL const& base_url, std::string_view const& path, std::string_view const& method, int headers_idx, std::string_view const& mimetype, std::string_view const& body)
{
	util::BlobBuffer buffer;
	buffer.reserve(1024 + body.size());

	std::string_view const& base_path = base_url.get_path();
	std::string_view request_path;
	if (base_path.ends_with('/') && path.starts_with('/'))
		request_path = path.substr(1);
	else
		request_path = path;

	buffer << method << ' ' << base_path << request_path << " HTTP/1.1" << g_header_newline;
	buffer << "Host: " << base_url.get_host() << g_header_newline
	       << "User-Agent: Deck-Assistant\r\n"
	          "Cache-Control: no-cache\r\n"
	          "Connection: keep-alive\r\n"sv;

	bool has_content_type = false;
	if (lua_istable(L, headers_idx))
	{
		int const table_idx = LuaHelpers::absidx(L, headers_idx);
		lua_pushnil(L);
		while (lua_next(L, table_idx))
		{
			std::string_view key   = LuaHelpers::to_string_view(L, -2);
			std::string_view value = LuaHelpers::to_string_view(L, -1);
			if (!key.empty() && !value.empty())
			{
				buffer << key << g_header_separator << value << g_header_newline;
				if (!has_content_type && key == g_content_type)
					has_content_type = true;
			}
			lua_pop(L, 1);
		}
	}

	if (!has_content_type && !mimetype.empty() && !body.empty())
		buffer << g_content_type << g_header_separator << mimetype << g_header_newline;

	buffer << "Content-Length: " << body.size() << "\r\n\r\n";

	if (!body.empty())
		buffer << body;

	return buffer;
}

inline util::BlobBuffer compose_payload(lua_State* L, util::URL const& base_url, std::string_view const& path, std::string_view const& method, int headers_idx)
{
	return compose_payload(L, base_url, path, method, headers_idx, std::string_view(), std::string_view());
}

void push_error_response(lua_State* L, std::string_view const& err)
{
	lua_createtable(L, 0, 3);

	lua_pushboolean(L, false);
	lua_setfield(L, -2, "ok");

	lua_pushinteger(L, 500);
	lua_setfield(L, -2, "code");

	lua_pushlstring(L, err.data(), err.size());
	lua_setfield(L, -2, "error");
}

int convert_to_http_message(lua_State* L, std::string_view const& input, bool is_eof)
{
	std::string_view error;

	util::HttpMessage http = util::parse_http_message(input);
	if (!http.error.empty())
	{
		error = http.error;
	}
	else if (http.body_start == 0)
	{
		if (is_eof)
			error = "EOF before response finished";
	}
	else
	{
		std::size_t const body_size = input.size() - http.body_start;
		bool is_done                = false;

		auto found_content_length = http.headers.find("Content-Length");
		if (found_content_length == http.headers.cend())
		{
			is_done = is_eof;
		}
		else
		{
			int content_length = 0;
			char const* first  = found_content_length->second.data();
			char const* last   = first + found_content_length->second.size();
			auto result        = std::from_chars(first, last, content_length, 10);
			if (result.ec != std::errc() || result.ptr != last || content_length < 0)
			{
				error = "Invalid Content-Length in response";
			}
			else if (body_size < std::size_t(content_length))
			{
				if (is_eof)
					error = "EOF before response finished";
			}
			else
			{
				is_done = true;
			}
		}

		if (is_done)
		{
			lua_createtable(L, 0, 3);

			lua_pushboolean(L, true);
			lua_setfield(L, -2, "ok");

			lua_pushinteger(L, http.response_status_code);
			lua_setfield(L, -2, "code");

			lua_createtable(L, 0, http.headers.size());
			for (auto const& header : http.headers)
			{
				lua_pushlstring(L, header.first.data(), header.first.size());
				lua_pushlstring(L, header.second.data(), header.second.size());
				lua_rawset(L, -3);
			}
			lua_setfield(L, -2, "headers");

			if (body_size > 0)
			{
				lua_pushlstring(L, input.data() + http.body_start, body_size);
				lua_setfield(L, -2, "body");
			}

			return 1;
		}
	}

	if (!error.empty())
	{
		push_error_response(L, error);
		return -1;
	}

	return 0;
}

} // namespace

char const* ConnectorHttp::LUA_TYPENAME = "deck:ConnectorHttp";

ConnectorHttp::ConnectorHttp(std::shared_ptr<util::SocketSet> const& socketset)
    : m_socket(socketset)
    , m_request_started_at(0)
    , m_next_connect_attempt(0)
    , m_request_counter(0)
    , m_connect_attempts(0)
    , m_enabled(true)
    , m_insecure(false)
{
}

ConnectorHttp::~ConnectorHttp()
{
	m_enabled            = false;
	m_request_started_at = 0;
	while (!m_queue.empty())
		m_queue.pop();
}

void ConnectorHttp::tick_inputs(lua_State* L, lua_Integer clock)
{
	if (!m_request_started_at)
		return;

	switch (m_socket.get_state())
	{
		case util::Socket::State::Disconnected:
		case util::Socket::State::Connecting:
			return;

		case util::Socket::State::TLSHandshaking:
			m_socket.tls_handshake();
			return;

		case util::Socket::State::Connected:
			break;
	}

	if (m_response.space() < 1024)
		m_response.reserve(m_response.capacity() + 4096);

	int read_result  = m_socket.read_nonblock(m_response.tail(), m_response.space());
	int have_message = 0;

	if (read_result < 0)
		DeckLogger::log_message(L, DeckLogger::Level::Debug, "ConnectorHttp ", m_base_url.get_connection_string(), " connection closed by peer");

	if (read_result > 0)
	{
		m_response.added_to_tail(read_result);
		m_connect_attempts = 0;
	}

	if (read_result != 0)
		have_message = convert_to_http_message(L, m_response.to_bin(), read_result < 0);

	if (have_message == 0)
	{
		if (read_result < 0)
		{
			push_error_response(L, "Connection closed by peer");
			have_message = -1;
		}
		else if (clock > m_request_started_at + 2000)
		{
			DeckLogger::log_message(L, DeckLogger::Level::Debug, "ConnectorHttp ", m_base_url.get_connection_string(), " request timed out, closing socket");
			m_socket.shutdown();
			m_socket.close();
			m_connect_attempts = 0;

			push_error_response(L, "Request timed out");
			have_message = -1;
		}
	}

	if (have_message != 0)
	{
		m_next_connect_attempt = clock + 200;

		DeckPromiseList* promise_list = push_promise_list(L);
		lua_pushinteger(L, m_queue.front().promise);
		lua_pushvalue(L, -3);

		if (!promise_list->fulfill_promise(L))
		{
			if (have_message < 0)
			{
				lua_getfield(L, -2, "error");
				LuaHelpers::emit_event(L, 1, "on_request_failed", LuaHelpers::StackValue(L, -1));
			}
			else
			{
				lua_pushvalue(L, -2);
				LuaHelpers::emit_event(L, 1, "on_response", LuaHelpers::StackValue(L, -1));
			}
		}

		lua_pop(L, 3);

		m_response.clear();
		m_queue.pop();
		m_request_started_at = 0;
	}
}

void ConnectorHttp::tick_outputs(lua_State* L, lua_Integer clock)
{
	util::Socket::State const state = m_socket.get_state();

	switch (state)
	{
		case util::Socket::State::Disconnected:
			if (!m_queue.empty() && m_enabled && clock >= m_next_connect_attempt)
			{
				m_queue.front().payload.rewind();
				m_response.clear();

				std::string_view err = m_socket.get_last_error();

				++m_connect_attempts;
				if (m_connect_attempts > 1)
					DeckLogger::log_message(L, DeckLogger::Level::Warning, "ConnectorHttp ", m_base_url.get_connection_string(), " connection reset: ", err);

				if (m_connect_attempts > 3)
				{
					DeckLogger::log_message(L, DeckLogger::Level::Error, "ConnectorHttp ", m_base_url.get_connection_string(), " too many connection errors, connector paused");

					// TODO cancel all pending promises instead of just the front

					DeckPromiseList* promise_list = push_promise_list(L);
					push_error_response(L, err);

					if (!promise_list->fulfill_all_promises(L))
						LuaHelpers::emit_event(L, 1, "on_request_failed", err);

					lua_pop(L, 1);

					while (!m_queue.empty())
						m_queue.pop();

					m_connect_attempts     = 0;
					m_next_connect_attempt = clock + 6000;
				}
				else
				{
					bool const use_tls = m_base_url.get_schema() == "https";

					int port = m_base_url.get_port();
					if (!port)
						port = use_tls ? 443 : 80;

					util::Socket::TLS tls;
					if (!use_tls)
						tls = util::Socket::TLS::NoTLS;
					else if (m_insecure)
						tls = util::Socket::TLS::TLSNoVerify;
					else
						tls = util::Socket::TLS::TLS;

					DeckLogger::log_message(L, DeckLogger::Level::Debug, "ConnectorHttp ", m_base_url.get_connection_string(), " connecting to server");
					m_socket.set_tls(tls);
					m_socket.start_connect(m_base_url.get_host(), port);
					m_next_connect_attempt = clock + 1000;
				}
			}
			break;

		case util::Socket::State::Connecting:
			break;

		case util::Socket::State::TLSHandshaking:
			m_socket.tls_handshake();
			break;

		case util::Socket::State::Connected:
			if (m_queue.empty())
			{
				DeckLogger::log_message(L, DeckLogger::Level::Debug, "ConnectorHttp ", m_base_url.get_connection_string(), " queue empty, closing socket");
				m_socket.shutdown();
				m_socket.close();
			}
			else if (!m_enabled)
			{
				DeckLogger::log_message(L, DeckLogger::Level::Debug, "ConnectorHttp ", m_base_url.get_connection_string(), " disabled, closing socket");
				m_socket.shutdown();
				m_socket.close();
			}
			else
			{
				if (!m_request_started_at)
					m_request_started_at = clock;

				util::BlobBuffer& outbuf = m_queue.front().payload;
				if (!outbuf.empty())
				{
					m_socket.write(outbuf.data(), outbuf.size());
					outbuf.advance(outbuf.size());
				}
			}
			break;
	}
}

void ConnectorHttp::shutdown(lua_State* L)
{
	m_enabled            = false;
	m_request_started_at = 0;
	m_response.release();
	while (!m_queue.empty())
		m_queue.pop();

	m_socket.shutdown();
	m_socket.close();
}

void ConnectorHttp::init_class_table(lua_State* L)
{
	Super::init_class_table(L);

	lua_pushcfunction(L, &_lua_get);
	lua_setfield(L, -2, "get");

	lua_pushcfunction(L, &_lua_post);
	lua_setfield(L, -2, "post");
}

void ConnectorHttp::init_instance_table(lua_State* L)
{
	lua_pushlightuserdata(L, this);
	DeckPromiseList::push_new(L, 10000);
	lua_settable(L, -3);

	LuaHelpers::create_callback_warning(L, "on_request_failed");
	LuaHelpers::create_callback_warning(L, "on_response");
}

int ConnectorHttp::index(lua_State* L, std::string_view const& key) const
{
	if (key == "enabled")
	{
		lua_pushboolean(L, m_enabled);
	}
	else if (key == "host")
	{
		std::string_view const value = m_base_url.get_host();
		lua_pushlstring(L, value.data(), value.size());
	}
	else if (key == "port")
	{
		int const value = m_base_url.get_port();
		if (value != 0)
			lua_pushinteger(L, value);
		else if (m_base_url.get_schema() == "https")
			lua_pushinteger(L, 443);
		else
			lua_pushinteger(L, 80);
	}
	else if (key == "path")
	{
		std::string_view const value = m_base_url.get_path();
		lua_pushlstring(L, value.data(), value.size());
	}
	else if (key == "insecure")
	{
		bool const use_tls = (m_base_url.get_schema() == "https");
		lua_pushboolean(L, !use_tls || m_insecure);
	}
	else if (key == "tls")
	{
		bool const use_tls = (m_base_url.get_schema() == "https");
		lua_pushboolean(L, use_tls);
	}
	else if (key == "connection_string")
	{
		std::string_view const value = m_base_url.get_connection_string();
		lua_pushlstring(L, value.data(), value.size());
	}
	return lua_gettop(L) == 2 ? 0 : 1;
}

int ConnectorHttp::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "enabled")
	{
		luaL_checktype(L, 3, LUA_TBOOLEAN);
		m_enabled = lua_toboolean(L, 3);
	}
	else if (key == "host")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, 3);
		if (!m_base_url.set_host(value))
			luaL_argerror(L, 3, "invalid value for host");
	}
	else if (key == "port")
	{
		int port = LuaHelpers::check_arg_int(L, 3);
		if (!m_base_url.set_port(port))
			luaL_argerror(L, 3, "invalid value for port");
	}
	else if (key == "path")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, 3);
		if (!m_base_url.set_path(value))
			luaL_argerror(L, 3, "invalid value for path");
	}
	else if (key == "insecure")
	{
		bool value = LuaHelpers::check_arg_bool(L, 3);
		m_insecure = value;
	}
	else if (key == "tls")
	{
		bool value = LuaHelpers::check_arg_bool(L, 3);
		m_base_url.set_schema(value ? "https" : "http");
	}
	else if (key == "connection_string" || key == "base_url")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, 3);

		util::URL new_url;
		if (!new_url.set_connection_string(value, "https"))
			luaL_argerror(L, 3, "connection string parsing failed");

		std::string_view const& schema = new_url.get_schema();
		if (schema != "https" && schema != "http")
			luaL_error(L, "invalid schema for websocket connections");

		m_base_url = std::move(new_url);
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

int ConnectorHttp::_lua_get(lua_State* L)
{
	ConnectorHttp* self           = from_stack(L, 1);
	std::string_view request_path = LuaHelpers::check_arg_string(L, 2);
	bool const have_headers       = lua_istable(L, 3);
	luaL_argcheck(L, have_headers || lua_isnone(L, 3), 3, "GET headers must be a table");
	luaL_checktype(L, 4, LUA_TNONE);

	util::BlobBuffer payload = compose_payload(L, self->m_base_url, request_path, "GET", 3);
	assert(self->queue_request(L, std::move(payload)));

	if (!self->m_enabled)
		DeckLogger::lua_log_message(L, DeckLogger::Level::Warning, "HttpConnector request queued but connector is disabled");

	return 1;
}

int ConnectorHttp::_lua_post(lua_State* L)
{
	ConnectorHttp* self           = from_stack(L, 1);
	std::string_view request_path = LuaHelpers::check_arg_string(L, 2);
	luaL_checktype(L, 3, LUA_TTABLE);
	int const vtype = lua_type(L, 4);
	luaL_argcheck(L, (vtype == LUA_TTABLE || vtype == LUA_TSTRING), 4, "POST payload must be a string or table");
	luaL_checktype(L, 5, LUA_TNONE);

	std::string_view mimetype;
	std::string_view body;
	std::string body_buffer;

	if (vtype == LUA_TSTRING)
	{
		mimetype = "text/plain; charset=UTF-8";
		body     = LuaHelpers::to_string_view(L, 4);
	}
	else
	{
		mimetype    = "application/json; charset=UTF-8";
		body_buffer = util::convert_to_json(L, 4, false);
		body        = body_buffer;
	}

	util::BlobBuffer payload = compose_payload(L, self->m_base_url, request_path, "POST", 3, mimetype, body);
	assert(self->queue_request(L, std::move(payload)));

	if (!self->m_enabled)
		DeckLogger::lua_log_message(L, DeckLogger::Level::Warning, "HttpConnector request queued but connector is disabled");

	return 1;
}

int ConnectorHttp::queue_request(lua_State* L, util::BlobBuffer&& payload)
{
	++m_request_counter;

	// Offset of magic 100 to force luajit to use nrec instead of narr
	// To prevent it from growing narr to infinity
	int const promise_idx = m_request_counter + 100;

	DeckPromiseList* promises = push_promise_list(L);
	lua_pushinteger(L, promise_idx);

	int result = promises->new_promise(L);
	if (!result)
	{
		lua_pop(L, 1);
		return 0;
	}
	lua_replace(L, -2);

	Request req;
	req.payload = std::move(payload);
	req.promise = promise_idx;
	m_queue.emplace(std::move(req));

	return 1;
}
