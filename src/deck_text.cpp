#include "deck_text.h"
#include "lua_class.hpp"

template class LuaClass<DeckText>;

char const* DeckText::LUA_TYPENAME = "deck:Text";

DeckText::DeckText(SDL_Surface* surface)
    : m_surface(surface)
{
	assert(m_surface && "invalid surface for DeckText");
}

DeckText::~DeckText()
{
	if (m_surface)
		SDL_free(m_surface);
}

int DeckText::index(lua_State* L, std::string_view const& key) const
{
	if (key == "w" || key == "width")
	{
		lua_pushinteger(L, m_surface->w);
	}
	else if (key == "h" || key == "height")
	{
		lua_pushinteger(L, m_surface->h);
	}
	else
	{
		luaL_error(L, "invalid key for DeckText (allowed: width, height)");
		lua_pushnil(L);
	}
	return 1;
}

int DeckText::tostring(lua_State* L) const
{
	lua_pushfstring(L, "%s { width=%d, height=%d }", LUA_TYPENAME, m_surface->w, m_surface->h);
	return 1;
}
