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

#ifndef DECK_ASSISTANT_DECK_PROMISE_H
#define DECK_ASSISTANT_DECK_PROMISE_H

#include "lua_class.h"
#include <SDL_rect.h>
#include <string_view>

class DeckPromise : public LuaClass<DeckPromise>
{
public:
	DeckPromise(int timeout) noexcept;

	bool check_wakeup(lua_Integer clock);
	bool mark_as_fulfilled();

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L);
	int tostring(lua_State* L) const;

private:
	static int _lua_fulfill(lua_State* L);
	static int _lua_reset(lua_State* L);
	static int _lua_wait(lua_State* L);

private:
	lua_Integer m_time_promised;
	lua_Integer m_time_fulfilled;
	lua_Integer m_timeout;
};

#endif // DECK_ASSISTANT_DECK_PROMISE_H
