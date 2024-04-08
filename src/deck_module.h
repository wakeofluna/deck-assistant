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

#ifndef DECK_ASSISTANT_DECK_MODULE_H
#define DECK_ASSISTANT_DECK_MODULE_H

#include "lua_class.h"
#include <optional>
#include <string_view>

class DeckConnectorContainer;

class DeckModule : public LuaClass<DeckModule>
{
public:
	DeckModule();

	static char const* LUA_TYPENAME;
	static constexpr bool const LUA_IS_GLOBAL = true;

	static lua_Integer get_clock(lua_State* L);

	void tick_inputs(lua_State* L, lua_Integer clock);
	void tick_outputs(lua_State* L);
	void shutdown(lua_State* L);

	void set_exit_requested(int exit_code);
	bool is_exit_requested() const;
	int get_exit_code() const;

	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L);

private:
	static int _lua_create_card(lua_State* L);
	static int _lua_create_colour(lua_State* L);
	static int _lua_create_connector(lua_State* L);
	static int _lua_create_font(lua_State* L);
	static int _lua_create_image(lua_State* L);
	static int _lua_create_promise_list(lua_State* L);
	static int _lua_create_rectangle(lua_State* L);
	static int _lua_create_rectangle_list(lua_State* L);
	static int _lua_request_quit(lua_State* L);

private:
	lua_Integer m_last_clock;
	lua_Integer m_last_delta;
	std::optional<int> m_exit_requested;
};

#endif // DECK_ASSISTANT_DECK_MODULE_H
