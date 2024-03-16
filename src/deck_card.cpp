#include "deck_card.h"
#include "deck_colour.h"
#include "deck_image.h"
#include "deck_text.h"
#include "lua_class.hpp"

template class LuaClass<DeckCard>;

char const* DeckCard::LUA_TYPENAME = "deck:Card";

DeckCard::DeckCard(int width, int height)
    : m_surface(nullptr)
{
	m_surface = SDL_CreateRGBSurfaceWithFormat(SDL_SIMD_ALIGNED, width, height, 32, SDL_PIXELFORMAT_ARGB32);
	SDL_SetSurfaceBlendMode(m_surface, SDL_BLENDMODE_BLEND);
}

DeckCard::~DeckCard()
{
	if (m_surface)
		SDL_FreeSurface(m_surface);
}

void DeckCard::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &_lua_blit);
	lua_setfield(L, -2, "blit");

	lua_pushcfunction(L, &_lua_clear);
	lua_setfield(L, -2, "clear");
}

int DeckCard::index(lua_State* L, std::string_view const& key) const
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
		luaL_error(L, "invalid key for DeckCard (allowed: width, height)");
		lua_pushnil(L);
	}
	return 1;
}

int DeckCard::tostring(lua_State* L) const
{
	lua_pushfstring(L, "%s { width=%d, height=%d }", LUA_TYPENAME, m_surface->w, m_surface->h);
	return 1;
}

int DeckCard::_lua_blit(lua_State* L)
{
	DeckCard* self = from_stack(L, 1);

	SDL_Surface* source = nullptr;
	int x               = 0;
	int y               = 0;
	int w               = 0;
	int h               = 0;

	if (DeckCard* card = DeckCard::from_stack(L, 2, false); card)
	{
		source = card->get_surface();
	}
	else if (DeckText* text = DeckText::from_stack(L, 2, false); text)
	{
		source = text->get_surface();
	}
	else if (DeckImage* img = DeckImage::from_stack(L, 2, false); img)
	{
		source = img->get_surface();
	}
	else
	{
		luaL_typerror(L, 2, "Card, Text or Image");
	}

	if (lua_gettop(L) >= 4)
	{
		luaL_argcheck(L, lua_type(L, 3) == LUA_TNUMBER, 3, "X coordinate must be an integer");
		luaL_argcheck(L, lua_type(L, 4) == LUA_TNUMBER, 4, "Y coordinate must be an integer");
		x = lua_tointeger(L, 3);
		y = lua_tointeger(L, 4);
	}

	if (lua_gettop(L) >= 6)
	{
		luaL_argcheck(L, lua_type(L, 5) == LUA_TNUMBER, 3, "WIDTH must be an integer");
		luaL_argcheck(L, lua_type(L, 6) == LUA_TNUMBER, 4, "HEIGHT must be an integer");
		w = lua_tointeger(L, 5);
		h = lua_tointeger(L, 6);
	}

	if (source)
	{
		if (!w)
			w = source->w;
		if (!h)
			h = source->h;

		SDL_Rect dstrect { x, y, w, h };
		SDL_BlitScaled(source, nullptr, self->m_surface, &dstrect);
	}

	return 0;
}

int DeckCard::_lua_clear(lua_State* L)
{
	DeckCard* self     = from_stack(L, 1);
	DeckColour* colour = DeckColour::from_stack(L, 2);
	SDL_FillRect(self->m_surface, nullptr, colour->get_colour().value);
	return 0;
}
