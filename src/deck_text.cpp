#include "deck_text.h"
#include "lua_class.hpp"

template class LuaClass<DeckText>;

char const* DeckText::LUA_TYPENAME = "deck-assistant.DeckText";

void DeckText::convert_top_of_stack(lua_State* L)
{
	newindex_type_or_convert(L, type_name(), &DeckText::push_new, "text");
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

int DeckText::to_string(lua_State* L) const
{
	lua_pushfstring(L, "%s: %p: \"%s\"", type_name(), this, m_text.c_str());
	return 1;
}
