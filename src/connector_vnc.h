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

#ifndef DECK_ASSISTANT_CONNECTOR_VNC_H
#define DECK_ASSISTANT_CONNECTOR_VNC_H

#ifdef HAVE_VNC

#include "connector_base.h"
#include <SDL.h>
#include <array>
#include <string>
#include <vector>

class DeckCard;
class DeckRectangleList;

struct _rfbScreenInfo;
typedef struct _rfbScreenInfo* rfbScreenInfoPtr;

class ConnectorVnc : public ConnectorBase<ConnectorVnc>
{
public:
	ConnectorVnc();
	~ConnectorVnc();

	void tick_inputs(lua_State* L, lua_Integer clock) override;
	void tick_outputs(lua_State* L) override;
	void shutdown(lua_State* L) override;

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);

	void notify_resize_request(int new_width, int new_height);
	void notify_ptr_event(int button_mask, int x, int y);

private:
	void pump_events();
	void close_vnc();

	static int _lua_redraw(lua_State* L);

private:
	rfbScreenInfoPtr m_screen_info;
	SDL_Surface* m_screen_surface;
	int m_screen_width;
	int m_screen_height;

	struct PointerState
	{
		int button_mask;
		int x;
		int y;
	};
	PointerState m_pointer_state;
	std::vector<PointerState> m_pointer_events;

	std::array<char, 64> m_title;
	std::array<char, 64> m_password;
	std::array<char, 64> m_bind_address;
	int m_bind_port;

	// Deck-related
	std::vector<bool> m_dirty_flags;
	DeckCard* m_card;
};

#endif

#endif // DECK_ASSISTANT_CONNECTOR_VNC_H
