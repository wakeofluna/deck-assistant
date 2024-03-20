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
				result += m_colour.r;
				break;
			case 'g':
				result += m_colour.g;
				break;
			case 'b':
				result += m_colour.b;
				break;
			case 'a':
				result += m_colour.a;
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
				m_colour.r = value & 0xff;
				break;
			case 'g':
				m_colour.g = value & 0xff;
				break;
			case 'b':
				m_colour.b = value & 0xff;
				break;
			case 'a':
				m_colour.a = value & 0xff;
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
	lua_pushfstring(L, "%s { r=%d, g=%d, b=%d, a=%d }", LUA_TYPENAME, int(m_colour.r), int(m_colour.g), int(m_colour.b), int(m_colour.a));
	return 1;
}
