#include "deck_rectangle.h"

char const* DeckRectangle::LUA_TYPENAME = "deck:Rectangle";

DeckRectangle::DeckRectangle()
    : m_rectangle {}
{
}

DeckRectangle::DeckRectangle(int w, int h)
    : m_rectangle { 0, 0, w, h }
{
}

DeckRectangle::DeckRectangle(int x, int y, int w, int h)
    : m_rectangle { x, y, w, h }
{
}

int DeckRectangle::index(lua_State* L, std::string_view const& key) const
{
	if (key == "x" || key == "left")
	{
		lua_pushinteger(L, m_rectangle.x);
	}
	else if (key == "y" || key == "top")
	{
		lua_pushinteger(L, m_rectangle.y);
	}
	else if (key == "w" || key == "width")
	{
		lua_pushinteger(L, m_rectangle.w);
	}
	else if (key == "h" || key == "height")
	{
		lua_pushinteger(L, m_rectangle.h);
	}
	else
	{
		luaL_error(L, "invalid key for Rectangle (allowed: x, y, w, h)");
		lua_pushnil(L);
	}
	return 1;
}

int DeckRectangle::newindex(lua_State* L, std::string_view const& key)
{
	int value = check_arg_int(L, 3);

	if (key == "x" || key == "left")
	{
		m_rectangle.x = value;
	}
	else if (key == "y" || key == "top")
	{
		m_rectangle.y = value;
	}
	else if (key == "w" || key == "width")
	{
		luaL_argcheck(L, (value >= 0), 3, "value must be positive");
		m_rectangle.w = value;
	}
	else if (key == "h" || key == "height")
	{
		luaL_argcheck(L, (value >= 0), 3, "value must be positive");
		m_rectangle.h = value;
	}
	else
	{
		luaL_error(L, "invalid key for Rectangle (allowed: x, y, w, h)");
	}
	return 0;
}

int DeckRectangle::tostring(lua_State* L) const
{
	lua_pushfstring(L, "%s { x=%d, y=%d, w=%d, h=%d }", LUA_TYPENAME, int(m_rectangle.x), int(m_rectangle.y), int(m_rectangle.w), int(m_rectangle.h));
	return 1;
}
