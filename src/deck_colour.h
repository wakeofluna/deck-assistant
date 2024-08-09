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

#ifndef DECK_ASSISTANT_DECK_COLOUR_H
#define DECK_ASSISTANT_DECK_COLOUR_H

#include "lua_class.h"
#include "util_colour.h"

class DeckColour : public LuaClass<DeckColour>
{
public:
	DeckColour();
	DeckColour(util::Colour c);

	inline util::Colour get_colour() const { return m_colour; }

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);

	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);
	int tostring(lua_State* L) const;

private:
	static int _lua_darken(lua_State* L);
	static int _lua_desaturate(lua_State* L);
	static int _lua_fade_to(lua_State* L);
	static int _lua_lighten(lua_State* L);

private:
	util::Colour m_colour;
};

#endif // DECK_ASSISTANT_DECK_COLOUR_H
