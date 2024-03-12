#include "deck_font.h"
#include "lua_class.hpp"

template class LuaClass<DeckFont>;

char const* DeckFont::LUA_TYPENAME = "deck:Font";

DeckFont::DeckFont()
    : m_font_name("Sans")
    , m_font_size(12)
{
}

DeckFont::DeckFont(std::string_view const& value)
{
	std::size_t maybe_size = 0;
	std::size_t factor     = 1;
	std::size_t offset     = value.size();
	while (offset > 0)
	{
		--offset;
		char const ch = value[offset];

		if (ch >= '0' && ch <= '9')
		{
			maybe_size += (ch - '0') * factor;
			factor     *= 10;
		}
		else if (ch == ' ')
		{
			break;
		}
		else
		{
			maybe_size = 0;
			offset     = value.size();
			break;
		}
	}

	m_font_size = maybe_size == 0 ? 12 : maybe_size;
	m_font_name = value.substr(0, offset);
}

int DeckFont::index(lua_State* L, std::string_view const& key) const
{
	if (key == "font")
	{
		lua_pushlstring(L, m_font_name.c_str(), m_font_name.size());
	}
	else if (key == "size")
	{
		lua_pushinteger(L, m_font_size);
	}
	else
	{
		luaL_error(L, "invalid key for DeckFont (allowed: font, size)");
	}
	return 1;
}

int DeckFont::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "font")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, -1);
		m_font_name            = value;
	}
	else if (key == "size")
	{
		int value   = LuaHelpers::check_arg_int(L, -1);
		m_font_size = value;
	}
	else
	{
		luaL_argerror(L, absidx(L, -2), "invalid key for DeckFont (allowed: font, size)");
	}
	return 0;
}

int DeckFont::tostring(lua_State* L) const
{
	lua_pushfstring(L, "%s { font='%s', size=%d }", LUA_TYPENAME, m_font_name.c_str(), int(m_font_size));
	return 1;
}
