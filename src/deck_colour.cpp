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

#include "deck_colour.h"
#include "lua_helpers.h"

char const* DeckColour::LUA_TYPENAME = "deck:Colour";

DeckColour::DeckColour()
{
	m_colour.clear();
}

DeckColour::DeckColour(Colour c)
    : m_colour(c)
{
}

int DeckColour::index(lua_State* L, std::string_view const& key) const
{
	std::uint64_t result = 0;

	for (char ch : key)
	{
		result <<= 8;
		switch (ch)
		{
			case 'r':
				result += m_colour.color.r;
				break;
			case 'g':
				result += m_colour.color.g;
				break;
			case 'b':
				result += m_colour.color.b;
				break;
			case 'a':
				result += m_colour.color.a;
				break;
			default:
				luaL_argerror(L, 2, "invalid colour key index");
				break;
		}
	}

	lua_pushinteger(L, result);
	return 1;
}

int DeckColour::newindex(lua_State* L, std::string_view const& key)
{
	lua_Integer given_value = LuaHelpers::check_arg_int(L, 3);
	if (given_value < 0)
		luaL_argerror(L, 3, "colour value cannot be negative");

	std::uint64_t value = given_value;
	for (size_t idx = key.size(); idx > 0; --idx)
	{
		char ch = key[idx - 1];
		switch (ch)
		{
			case 'r':
				m_colour.color.r = value & 0xff;
				break;
			case 'g':
				m_colour.color.g = value & 0xff;
				break;
			case 'b':
				m_colour.color.b = value & 0xff;
				break;
			case 'a':
				m_colour.color.a = value & 0xff;
				break;
			default:
				luaL_argerror(L, 2, "invalid colour key index");
				break;
		}
		value >>= 8;
	}

	return 0;
}

int DeckColour::tostring(lua_State* L) const
{
	lua_pushfstring(L, "%s { r=%d, g=%d, b=%d, a=%d }", LUA_TYPENAME, int(m_colour.color.r), int(m_colour.color.g), int(m_colour.color.b), int(m_colour.color.a));
	return 1;
}
