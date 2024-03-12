#include "deck_text.h"
#include "lua_class.hpp"

template class LuaClass<DeckText>;

char const* DeckText::LUA_TYPENAME = "deck:Text";

DeckText::DeckText() = default;

DeckText::DeckText(std::string_view const& value)
    : m_text(value)
{
}

int DeckText::index(lua_State* L, std::string_view const& key) const
{
	if (key == "text")
	{
		lua_pushlstring(L, m_text.c_str(), m_text.size());
	}
	else if (key == "font")
	{
		if (m_font.empty())
			lua_pushliteral(L, "default");
		else
			lua_pushlstring(L, m_font.c_str(), m_font.size());
	}
	else
	{
		luaL_error(L, "invalid key for DeckText (allowed: text, font)");
		lua_pushnil(L);
	}
	return 1;
}

int DeckText::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "text")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, 3);
		m_text                 = value;
	}
	else if (key == "font")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, 3);
		m_font                 = value;
	}
	else
	{
		luaL_error(L, "invalid key for DeckText (allowed: text, font)");
	}
	return 0;
}

int DeckText::tostring(lua_State* L) const
{
	lua_pushfstring(L, "%s { text='%s', font='%s' }", LUA_TYPENAME, m_text.c_str(), m_font.c_str());
	return 1;
}
