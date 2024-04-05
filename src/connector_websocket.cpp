#include "connector_websocket.h"
#include "deck_logger.h"
#include "lua_helpers.h"
#include "util_blob.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <random>

namespace
{

enum CloseReason : std::uint16_t
{
	CLOSE_None            = 0,
	CLOSE_Normal          = 1000,
	CLOSE_GoingAway       = 1001,
	CLOSE_ProtocolError   = 1002,
	CLOSE_Unacceptable    = 1003,
	CLOSE_InvalidFormat   = 1007,
	CLOSE_PolicyViolated  = 1008,
	CLOSE_MessageTooLarge = 1009,
	CLOSE_ExtensionFailed = 1010,
	CLOSE_InternalError   = 1011,
	CLOSE_HandshakeFailed = 1015,
};

#ifdef HAVE_GNUTLS

Blob make_websocket_key_nonce()
{
	return Blob::from_random(16);
}

Blob make_websocket_accept_nonce(Blob const& key)
{
	constexpr std::string_view websocket_uuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

	Blob blob(60);
	blob += key.to_base64();
	blob += websocket_uuid;
	return blob.sha1();
}

#else

// Hardcoded due to absence of SHA1

Blob make_websocket_key_nonce()
{
	return Blob::from_literal("DeckAssistantWS!");
}

Blob make_websocket_accept_nonce(Blob const&)
{
	bool ok;
	return Blob::from_base64("H3P/IhOIXOMiE9YV+WG5Jqe3G08=", ok);
}

#endif

} // namespace

char const* ConnectorWebsocket::LUA_TYPENAME = "deck:ConnectorWebsocket";

ConnectorWebsocket::ConnectorWebsocket()
    : m_connect_last_attempt(-5000)
    , m_connect_state(State::Disconnected)
    , m_enabled(true)
    , m_close_sent(false)
    , m_pending_opcode(0)
    , m_random(std::chrono::system_clock::now().time_since_epoch().count())
{
	m_connect_url.set_schema("ws");
	m_receive_buffer.assign(4096, '\0');
	m_received.reserve(8192);
}

ConnectorWebsocket::~ConnectorWebsocket()
{
}

void ConnectorWebsocket::tick_inputs(lua_State* L, lua_Integer clock)
{
	if (m_connect_state == State::Disconnected)
	{
		if (!m_enabled || clock < m_connect_last_attempt + 5000)
			return;

		m_connect_last_attempt = clock;
		m_close_sent           = false;

		std::string_view host = m_connect_url.get_host();
		int port              = m_connect_url.get_port();

		if (host.empty())
			host = "localhost";

		m_connect_state = State::Connecting;
		m_socket.start_connect(host, port);
	}

	if (m_connect_state == State::Connecting)
	{
		Socket::State const socket_state = m_socket.get_state();
		switch (socket_state)
		{
			case Socket::State::Disconnected:
				m_connect_state = State::Disconnected;
				DeckLogger::log_message(L, DeckLogger::Level::Warning, "Websocket connection failed: ", m_socket.get_last_error());
				LuaHelpers::emit_event(L, 1, "on_connect_failed", m_socket.get_last_error());
				return;

			case Socket::State::Connecting:
				return;

			case Socket::State::Connected:
				DeckLogger::log_message(L, DeckLogger::Level::Debug, "Websocket connected, starting handshake");
				std::string_view host = m_connect_url.get_host();

				m_websocket_key = make_websocket_key_nonce();

				std::string http;
				http.reserve(512);
				http  = "GET ";
				http += m_connect_url.get_path();
				http += " HTTP/1.1\r\n";
				http += "Host: ";
				http += host.empty() ? "localhost" : host;
				http += ':';
				http += std::to_string(m_connect_url.get_port());
				http += "\r\n";
				http += "Connection: upgrade\r\n";
				http += "Upgrade: websocket\r\n";
				http += "Sec-WebSocket-Version: 13\r\n";
				http += "Sec-WebSocket-Key: ";
				http += m_websocket_key.to_base64();
				http += "\r\n";
				if (!m_accepted_protocols.empty())
				{
					http += "Sec-WebSocket-Protocol: ";
					http += m_accepted_protocols;
					http += "\r\n";
				}
				http += "\r\n";

				m_socket.write(http.data(), http.size());
				m_connect_state = State::Handshaking;
				break;
		}
	}

	int received = m_socket.read_nonblock(m_receive_buffer.data(), m_receive_buffer.size());
	if (received < 0)
	{
		m_connect_state = State::Disconnected;
		m_received.clear();
		DeckLogger::log_message(L, DeckLogger::Level::Error, "Websocket disconnected: ", m_socket.get_last_error());

		char const* function_name = (m_connect_state == State::Handshaking) ? "on_connect_failed" : "on_disconnect";
		LuaHelpers::emit_event(L, 1, function_name, m_socket.get_last_error());

		return;
	}

	if (received == 0)
		return;

	std::string_view received_data(m_receive_buffer.data(), received);
	DeckLogger::log_message(L, DeckLogger::Level::Debug, "== Received ", received, " bytes from socket ==");
	// DeckLogger::log_message(L, DeckLogger::Level::Debug, received_data);

	m_received += received_data;

	if (m_connect_state == State::Handshaking)
	{
		std::size_t headers_end = m_received.find("\r\n\r\n");
		if (headers_end == std::string::npos)
		{
			if (m_received.size() > 2048)
			{
				m_socket.close();
				m_received.clear();
				m_connect_state = State::Disconnected;

				DeckLogger::log_message(L, DeckLogger::Level::Error, "Websocket upgrade failed");
				LuaHelpers::emit_event(L, 1, "on_connect_failed", "Websocket upgrade failed");
			}
			return;
		}

		std::string_view headers = std::string_view(m_received).substr(0, headers_end + 2);
		if (!verify_http_upgrade_headers(headers))
		{
			m_socket.close();
			m_received.clear();
			m_connect_state = State::Disconnected;
			DeckLogger::log_message(L, DeckLogger::Level::Error, "Websocket upgrade failed");
			LuaHelpers::emit_event(L, 1, "on_connect_failed", "Websocket upgrade failed");
			return;
		}

		m_received      = m_received.substr(headers_end + 4);
		m_connect_state = State::Connected;

		DeckLogger::log_message(L, DeckLogger::Level::Debug, "Websocket handshake complete using protocol ", m_active_protocol);
		LuaHelpers::emit_event(L, 1, "on_connect", m_active_protocol);
	}

	Frame frame;
	std::uint16_t close_reason;
	while (check_for_complete_frame(frame, close_reason))
	{
		DeckLogger::log_message(L, DeckLogger::Level::Debug, "== Websocket frame with opcode ", int(frame.first), " ==");
		DeckLogger::log_message(L, DeckLogger::Level::Debug, frame.second);

		// Connection close
		if (frame.first == 8)
		{
			// Send confirmation
			if (!m_close_sent)
				send_close_frame(CLOSE_Normal);

			std::uint16_t close_code = CLOSE_None;
			std::string_view close_message;

			if (frame.second.size() >= 2)
				close_code = (frame.second[0] << 8) + frame.second[1];
			if (frame.second.size() > 2)
				close_message = frame.second.substr(2);

			if (close_code == CLOSE_None)
				close_code = CLOSE_Normal;

			m_socket.close();
			m_received.clear();
			m_connect_state = State::Disconnected;

			if (!m_close_sent)
			{
				std::string error_message  = "Websocket disconnected by remote host with code: ";
				error_message             += std::to_string(close_code);

				DeckLogger::log_message(L, DeckLogger::Level::Warning, error_message);
				if (!close_message.empty())
					DeckLogger::log_message(L, DeckLogger::Level::Warning, "Server message: ", close_message);

				LuaHelpers::emit_event(L, 1, "on_disconnect", error_message);
			}
			else
			{
				DeckLogger::log_message(L, DeckLogger::Level::Debug, "Websocket disabled, connection closed");
			}

			break;
		}

		// Ping
		if (frame.first == 9)
		{
			DeckLogger::log_message(L, DeckLogger::Level::Debug, "Websocket ping received");
			frame.first = 10;
			send_frame(frame);
			continue;
		}

		// Pong
		if (frame.first == 10)
		{
			DeckLogger::log_message(L, DeckLogger::Level::Debug, "Websocket pong received");
			continue;
		}

		// Forward to lua
		if (!m_close_sent && m_enabled)
			LuaHelpers::emit_event(L, 1, "on_message", frame.second, int(frame.first));
	}

	if (close_reason != CLOSE_None)
	{
		if (!m_close_sent)
		{
			send_close_frame(close_reason);
			m_close_sent = true;
		}

		std::string error_message  = "Websocket error, closing connection with code: ";
		error_message             += std::to_string(close_reason);

		DeckLogger::log_message(L, DeckLogger::Level::Error, error_message);
		LuaHelpers::emit_event(L, 1, "on_disconnect", error_message);
	}
}

void ConnectorWebsocket::tick_outputs(lua_State* L)
{
	// Noop since writes are blocking anyway
	if (m_connect_state == State::Connected && !m_enabled && !m_close_sent)
	{
		send_close_frame(CLOSE_Normal);
		m_close_sent = true;

		std::string_view error_message = "Websocket disabled, closing connection";
		DeckLogger::log_message(L, DeckLogger::Level::Debug, error_message);
		LuaHelpers::emit_event(L, 1, "on_disconnect", error_message);
	}
}

void ConnectorWebsocket::shutdown(lua_State* L)
{
	if (m_connect_state == State::Connected)
	{
		// Send a close frame. Note that officially we need to wait for a response but we're in a hurry here
		send_close_frame(CLOSE_GoingAway);
	}

	m_socket.close();
	m_received.clear();
	m_connect_state = State::Disconnected;
}

void ConnectorWebsocket::init_class_table(lua_State* L)
{
	Super::init_class_table(L);

	lua_pushcfunction(L, &_lua_send_message);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "write");
	lua_setfield(L, -2, "send");
}

void ConnectorWebsocket::init_instance_table(lua_State* L)
{
}

int ConnectorWebsocket::index(lua_State* L, std::string_view const& key) const
{
	if (key == "error")
	{
		std::string_view error = m_socket.get_last_error();
		lua_pushlstring(L, error.data(), error.size());
	}
	else if (key == "connected")
	{
		lua_pushboolean(L, m_connect_state == State::Connected);
	}
	else if (key == "enabled")
	{
		lua_pushboolean(L, m_enabled);
	}
	else if (key == "host")
	{
		std::string_view const value = m_connect_url.get_host();
		lua_pushlstring(L, value.data(), value.size());
	}
	else if (key == "port")
	{
		int const value = m_connect_url.get_port();
		if (value != 0)
			lua_pushinteger(L, value);
		else if (m_connect_url.get_schema() == "wss")
			lua_pushinteger(L, 443);
		else
			lua_pushinteger(L, 80);
	}
	else if (key == "path")
	{
		std::string_view const value = m_connect_url.get_path();
		lua_pushlstring(L, value.data(), value.size());
	}
	else if (key == "secure")
	{
		bool const is_secure = (m_connect_url.get_schema() == "wss");
		lua_pushboolean(L, is_secure);
	}
	else if (key == "connection_string")
	{
		std::string_view const value = m_connect_url.get_connection_string();
		lua_pushlstring(L, value.data(), value.size());
	}
	else if (key == "protocol")
	{
		lua_pushlstring(L, m_active_protocol.data(), m_active_protocol.size());
	}
	else if (key == "accepted_protocols" || key == "protocols")
	{
		lua_pushlstring(L, m_accepted_protocols.data(), m_accepted_protocols.size());
	}
	return lua_gettop(L) == 2 ? 0 : 1;
}

int ConnectorWebsocket::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "error" || key == "connected")
	{
		luaL_error(L, "key %s is readonly for %s", key.data(), LUA_TYPENAME);
	}
	else if (m_connect_state != State::Disconnected && key == "protocol")
	{
		luaL_error(L, "key %s is readonly for %s while connected to remote host", key.data(), LUA_TYPENAME);
	}
	else if (key == "enabled")
	{
		luaL_checktype(L, 3, LUA_TBOOLEAN);
		m_enabled = lua_toboolean(L, 3);
	}
	else if (key == "host")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, 3);
		if (!m_connect_url.set_host(value))
			luaL_argerror(L, 3, "invalid value for host");
	}
	else if (key == "port")
	{
		int port = LuaHelpers::check_arg_int(L, 3);
		if (!m_connect_url.set_port(port))
			luaL_argerror(L, 3, "invalid value for port");
	}
	else if (key == "path")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, 3);
		if (!m_connect_url.set_path(value))
			luaL_argerror(L, 3, "invalid value for path");
	}
	else if (key == "secure")
	{
		bool value = LuaHelpers::check_arg_bool(L, 3);
		luaL_argcheck(L, value == false, 3, "TLS connections are not (yet) supported");
		m_connect_url.set_schema(value ? "wss" : "ws");
	}
	else if (key == "connection_string")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, 3);

		URL new_url;
		if (!new_url.set_connection_string(std::string(value)))
			luaL_argerror(L, 3, "connection string parsing failed");

		std::string_view const& schema = new_url.get_schema();
		if (schema == "wss")
			luaL_error(L, "TLS connections are not (yet) supported");
		else if (schema != "ws")
			luaL_error(L, "invalid schema for websocket connections");

		m_connect_url = new_url;
	}
	else if (key == "accepted_protocols" || key == "protocols" || key == "protocol")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, 3, true);
		m_accepted_protocols   = value;
	}
	else if (key.starts_with("on_"))
	{
		luaL_argcheck(L, (lua_type(L, 3) == LUA_TFUNCTION), 3, "event handlers must be functions");
		LuaHelpers::newindex_store_in_instance_table(L);
	}
	else
	{
		LuaHelpers::newindex_store_in_instance_table(L);
	}
	return 0;
}

bool ConnectorWebsocket::verify_http_upgrade_headers(std::string_view const& headers_full)
{
	std::size_t offset = 0;
	std::size_t found;
	std::size_t const headers_end = headers_full.size();

	bool has_switch_protocol = false;
	bool has_connection      = false;
	bool has_upgrade         = false;
	bool has_accept          = false;
	std::string_view protocol;

	while (offset < headers_end)
	{
		found = headers_full.find("\r\n", offset);
		if (found == std::string_view::npos)
			found = headers_end;

		std::string_view header = headers_full.substr(offset, found - offset);
		offset                  = found + 2;

		if (header.starts_with("HTTP/1.1 "))
		{
			has_switch_protocol = header.starts_with("HTTP/1.1 101");
			continue;
		}

		std::size_t split = header.find(": ");
		if (split == std::string_view::npos)
			return false;

		std::string_view key   = header.substr(0, split);
		std::string_view value = header.substr(split + 2);

		if (key == "Connection")
		{
			if (value != "Upgrade")
				return false;

			has_connection = true;
		}

		if (key == "Upgrade")
		{
			if (value != "websocket")
				return false;

			has_upgrade = true;
		}

		if (key == "Sec-WebSocket-Accept")
		{
			Blob expected = make_websocket_accept_nonce(m_websocket_key);
			if (value != expected.to_base64())
				return false;

			has_accept = true;
		}

		if (key == "Sec-WebSocket-Protocol")
			protocol = value;
	}

	if (!has_switch_protocol || !has_connection || !has_upgrade || !has_accept)
		return false;

	m_active_protocol = protocol;
	return true;
}

bool ConnectorWebsocket::check_for_complete_frame(Frame& frame_data, std::uint16_t& close_reason)
{
	close_reason = CLOSE_None;

	if (m_received.size() < 4)
		return false;

	unsigned char* cursor = (unsigned char*)m_received.data();

	unsigned char fin    = *cursor & 0x80;
	unsigned char resv   = *cursor & 0x70;
	unsigned char opcode = *cursor & 0x0f;
	++cursor;
	unsigned char masked      = *cursor & 0x80;
	unsigned char payload_len = *cursor & 0xff;
	++cursor;

	if (resv != 0)
	{
		close_reason = CLOSE_ProtocolError;
		return false;
	}

	if (payload_len == 127)
	{
		close_reason = CLOSE_MessageTooLarge;
		return false;
	}

	std::uint16_t payload_real_len;
	std::uint32_t full_len = 2;

	if (payload_len == 126)
	{
		if (m_received.size() < 4)
			return false;

		payload_real_len = (*cursor << 8);
		++cursor;
		payload_real_len += *cursor;
		++cursor;

		full_len += 2;
	}
	else
	{
		payload_real_len = payload_len;
	}

	if (masked)
		full_len += 4;

	full_len += payload_real_len;

	if (m_received.size() < full_len)
		return false;

	if (opcode != 0)
	{
		m_pending_opcode = opcode;
		m_pending_frame.clear();
	}

	if (masked)
	{
		unsigned char mask[4];
		std::memcpy(mask, cursor, 4);
		cursor += 4;

		for (std::size_t i = 0; i < payload_real_len; ++i)
		{
			m_pending_frame.push_back(*cursor ^ mask[i & 3]);
			++cursor;
		}
	}
	else
	{
		std::copy_n(cursor, payload_real_len, std::back_inserter(m_pending_frame));
	}

	if (m_received.size() == full_len)
		m_received.clear();
	else
		m_received = m_received.substr(full_len);

	if (fin)
	{
		frame_data.first  = m_pending_opcode;
		frame_data.second = m_pending_frame;
		return true;
	}

	return false;
}

bool ConnectorWebsocket::send_frame(Frame const& frame)
{
	std::uint32_t const full_mask   = m_random();
	std::uint16_t const data_size   = frame.second.size();
	unsigned char const* const mask = (unsigned char*)&full_mask;

	std::vector<unsigned char> block;
	block.reserve(frame.second.size() + 8);

	block.push_back(0x80 + frame.first);

	if (data_size >= 126)
	{
		block.push_back(0xfe);
		block.push_back(data_size >> 8);
		block.push_back(data_size & 0xff);
	}
	else
	{
		block.push_back(0x80 + data_size);
	}

	block.push_back(mask[0]);
	block.push_back(mask[1]);
	block.push_back(mask[2]);
	block.push_back(mask[3]);

	for (std::uint16_t i = 0; i < data_size; ++i)
		block.push_back(frame.second[i] ^ mask[i & 3]);

	return m_socket.write(block.data(), block.size());
}

bool ConnectorWebsocket::send_close_frame(std::uint16_t close_code)
{
	char buf[2];
	buf[0] = close_code << 8;
	buf[1] = close_code & 0xff;

	Frame frame;
	frame.first  = 0x08;
	frame.second = buf;

	return send_frame(frame);
}

int ConnectorWebsocket::_lua_send_message(lua_State* L)
{
	ConnectorWebsocket* self = from_stack(L, 1);
	std::string_view message = LuaHelpers::check_arg_string(L, 2);

	Frame frame;
	frame.first  = 1;
	frame.second = message;
	bool success = self->send_frame(frame);

	lua_pushboolean(L, success);
	return 1;
}
