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

#ifndef DECK_ASSISTANT_CONNECTOR_WINDOW_H
#define DECK_ASSISTANT_CONNECTOR_WINDOW_H

#include "connector_base.h"
#include <SDL.h>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class DeckCard;
class DeckRectangleList;

class ConnectorWindow : public ConnectorBase<ConnectorWindow>
{
public:
	ConnectorWindow();
	~ConnectorWindow();

	void initial_setup(lua_State* L, bool is_reload) override;
	void tick_inputs(lua_State* L, lua_Integer clock) override;
	void tick_outputs(lua_State* L, lua_Integer clock) override;
	void shutdown(lua_State* L) override;

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);

private:
	bool attempt_create_window(lua_State* L);

	static int _lua_redraw(lua_State* L);

	static int _sdl_event_filter(void* userdata, SDL_Event* event);
	void handle_window_event(lua_State* L, SDL_WindowEvent const& event);
	void handle_motion_event(lua_State* L, SDL_MouseMotionEvent const& event);
	void handle_button_event(lua_State* L, SDL_MouseButtonEvent const& event);
	void handle_wheel_event(lua_State* L, SDL_MouseWheelEvent const& event);
	void handle_keyboard_event(lua_State* L, SDL_KeyboardEvent const& event);
	void handle_text_input_event(lua_State* L, SDL_TextInputEvent const& event);

private:
	// Window physicals
	SDL_Window* m_window;
	std::optional<std::string> m_wanted_title;
	std::optional<int> m_wanted_width;
	std::optional<int> m_wanted_height;
	std::optional<bool> m_wanted_visible;
	bool m_exit_on_close;

	std::recursive_mutex m_mutex;
	bool m_event_size_changed;
	bool m_event_surface_dirty;
	std::vector<SDL_Event> m_pending_events;

	// Deck-related
	DeckCard* m_card;
};

#endif // DECK_ASSISTANT_CONNECTOR_WINDOW_H
