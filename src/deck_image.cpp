#include "deck_image.h"
#include "lua_class.hpp"

template class LuaClass<DeckImage>;

char const* DeckImage::LUA_TYPENAME = "deck:Image";

DeckImage::DeckImage()
    : m_src_width(0)
    , m_src_height(0)
    , m_wanted_width(-1)
    , m_wanted_height(-1)
{
}

DeckImage::DeckImage(std::string_view const& value)
{
	m_src = value;
}

int DeckImage::index(lua_State* L, std::string_view const& key) const
{
	if (key == "src")
	{
		lua_pushlstring(L, m_src.data(), m_src.size());
	}
	else
	{
		luaL_error(L, "invalid key for DeckImage (allowed: src)");
	}
	return 1;
}

int DeckImage::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "src")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, -1);
		m_src                  = value;
	}
	else
	{
		luaL_argerror(L, absidx(L, -2), "invalid key for DeckImage (allowed: src)");
	}
	return 0;
}

int DeckImage::tostring(lua_State* L) const
{
	lua_pushfstring(L, "%s { src='%s' }", LUA_TYPENAME, m_src.c_str());
	return 1;
}
