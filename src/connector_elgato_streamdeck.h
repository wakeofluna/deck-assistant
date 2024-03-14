#ifndef DECK_ASSISTANT_CONNECTOR_ELGATO_STREAMDECK_H
#define DECK_ASSISTANT_CONNECTOR_ELGATO_STREAMDECK_H

#include "i_connector.h"
#include <SDL_hidapi.h>
#include <array>
#include <string>
#include <vector>

class ConnectorElgatoStreamDeck : public IConnector
{
public:
	ConnectorElgatoStreamDeck();
	~ConnectorElgatoStreamDeck();

	static char const* SUBTYPE_NAME;
	char const* get_subtype_name() const override;

	void tick(lua_State* L, int delta_msec) override;
	void shutdown(lua_State* L) override;

	void init_instance_table(lua_State* L) override;
	int index(lua_State* L) const override;
	int newindex(lua_State* L) override;

private:
	static int _lua_default_on_connect(lua_State* L);
	static int _lua_default_on_disconnect(lua_State* L);
	static int _lua_default_on_press(lua_State* L);
	static int _lua_default_on_release(lua_State* L);

private:
	void attempt_connect_device();
	void write_brightness(unsigned char value);
	bool update_button_state();
	void force_disconnect();

private:
	SDL_hid_device* m_hid_device;
	std::string m_last_error;
	std::string m_filter_serialnumber;
	int m_delta_since_last_scan;

	std::string m_serialnumber;
	int m_vid;
	int m_pid;

	unsigned char m_wanted_brightness;
	unsigned char m_actual_brightness;
	std::array<unsigned char, 1024> m_buffer;
	std::vector<bool> m_buttons_state;
	std::vector<bool> m_buttons_new_state;
};

#endif // DECK_ASSISTANT_CONNECTOR_ELGATO_STREAMDECK_H
