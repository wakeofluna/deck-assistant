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

#ifndef DECK_ASSISTANT_DECK_PROMISE_LIST_H
#define DECK_ASSISTANT_DECK_PROMISE_LIST_H

#include "lua_class.h"
#include <string_view>

class DeckPromiseList : public LuaClass<DeckPromiseList>
{
public:
	DeckPromiseList() noexcept;
	DeckPromiseList(int default_timeout) noexcept;

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L);
	int tostring(lua_State* L) const;

	int new_promise(lua_State* L, int timeout = -1) const;
	int fulfill_promise(lua_State* L) const;
	int fulfill_all_promises(lua_State* L) const;

private:
	static int _lua_new_promise(lua_State* L);
	static int _lua_fulfill_promise(lua_State* L);

private:
	int m_default_timeout;
};

#endif // DECK_ASSISTANT_DECK_PROMISE_LIST_H
