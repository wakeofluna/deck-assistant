#ifndef DECK_ASSISTANT_CONNECTOR_WEBSOCKET_H
#define DECK_ASSISTANT_CONNECTOR_WEBSOCKET_H

#include "connector_base.h"
#include "util_blob.h"
#include "util_socket.h"
#include "util_url.h"
#include <random>
#include <string>
#include <utility>

class ConnectorWebsocket : public ConnectorBase<ConnectorWebsocket>
{
public:
	ConnectorWebsocket();
	~ConnectorWebsocket();

	void tick_inputs(lua_State* L, lua_Integer clock) override;
	void tick_outputs(lua_State* L) override;
	void shutdown(lua_State* L) override;

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);

private:
	using Frame = std::pair<unsigned char, std::string_view>;

	bool verify_http_upgrade_headers(std::string_view const& headers);
	bool check_for_complete_frame(Frame& frame, std::uint16_t& close_reason);
	bool send_frame(Frame const& frame);
	bool send_close_frame(std::uint16_t close_code);

	static int _lua_send_message(lua_State* L);

private:
	enum class State : char
	{
		Disconnected,
		Connecting,
		Handshaking,
		Connected,
	};

	Socket m_socket;
	URL m_connect_url;
	lua_Integer m_connect_last_attempt;
	State m_connect_state;
	bool m_enabled;
	bool m_close_sent;
	std::string m_accepted_protocols;
	std::string m_active_protocol;
	std::string m_remote_server_name;
	std::string m_receive_buffer;
	std::string m_received;
	Blob m_websocket_key;
	unsigned char m_pending_opcode;
	std::string m_pending_frame;
	std::mt19937 m_random;
};

#endif // DECK_ASSISTANT_CONNECTOR_WEBSOCKET_H
