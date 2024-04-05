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

#ifndef DECK_ASSISTANT_CONNECTOR_ELGATO_STREAMDECK_H
#define DECK_ASSISTANT_CONNECTOR_ELGATO_STREAMDECK_H

#include "connector_base.h"
#include <SDL_hidapi.h>
#include <SDL_surface.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class ConnectorElgatoStreamDeck : public ConnectorBase<ConnectorElgatoStreamDeck>
{
public:
	ConnectorElgatoStreamDeck();
	~ConnectorElgatoStreamDeck();

	void tick_inputs(lua_State* L, lua_Integer clock) override;
	void tick_outputs(lua_State* L) override;
	void shutdown(lua_State* L) override;

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);

private:
	static int _lua_default_on_connect(lua_State* L);
	static int _lua_default_on_disconnect(lua_State* L);
	static int _lua_default_on_press(lua_State* L);
	static int _lua_default_on_release(lua_State* L);
	static int _lua_set_button(lua_State* L);

private:
	void attempt_connect_device();
	void write_brightness(unsigned char value);
	void write_image_data(unsigned char button, std::vector<unsigned char> const& bytes);
	void set_button(unsigned char button, SDL_Surface* surface);
	bool update_button_state();
	void force_disconnect();

private:
	SDL_hid_device* m_hid_device;
	std::string m_last_error;
	std::string m_filter_serialnumber;
	std::uint32_t m_hid_last_scan;

	std::string m_serialnumber;
	int m_vid;
	int m_pid;
	int m_button_size;

	unsigned char m_wanted_brightness;
	unsigned char m_actual_brightness;
	std::array<unsigned char, 1024> m_buffer;
	std::vector<bool> m_buttons_state;
	std::vector<bool> m_buttons_new_state;
	std::vector<std::pair<unsigned char, std::vector<unsigned char>>> m_buttons_image;
};

#endif // DECK_ASSISTANT_CONNECTOR_ELGATO_STREAMDECK_H
