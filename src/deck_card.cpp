#include "deck_card.h"
#include "deck_colour.h"
#include "lua_class.hpp"
#include <SDL_image.h>
#include <algorithm>

template class LuaClass<DeckCard>;

char const* DeckCard::LUA_TYPENAME = "deck:Card";

namespace
{

enum class Format : char
{
	BMP,
	PNG,
	JPEG,
};

Sint64 buffer_rwops_size(struct SDL_RWops* context)
{
	return -1;
}

Sint64 buffer_rwops_seek(struct SDL_RWops* context, Sint64 offset, int whence)
{
	return -1;
}

size_t buffer_rwops_read(struct SDL_RWops* context, void* ptr, size_t size, size_t maxnum)
{
	return 0;
}

size_t buffer_rwops_write(struct SDL_RWops* context, void const* ptr, size_t size, size_t num)
{
	std::vector<unsigned char>* buffer = reinterpret_cast<std::vector<unsigned char>*>(context->hidden.unknown.data1);

	size_t total_size = size * num;
	std::copy_n((unsigned char*)ptr, total_size, std::back_inserter(*buffer));

	return total_size;
}

int buffer_rwops_close(struct SDL_RWops* context)
{
	if (context)
		SDL_FreeRW(context);
	return 0;
}

std::vector<unsigned char> save_surface_as(SDL_Surface* surface, Format format)
{
	std::vector<unsigned char> buffer;

	if (!surface)
		return buffer;

	buffer.reserve(surface->w * surface->h);

	SDL_RWops* ops            = SDL_AllocRW();
	ops->hidden.unknown.data1 = &buffer;
	ops->size                 = &buffer_rwops_size;
	ops->seek                 = &buffer_rwops_seek;
	ops->read                 = &buffer_rwops_read;
	ops->write                = &buffer_rwops_write;
	ops->close                = &buffer_rwops_close;

	if (format == Format::BMP)
	{
		SDL_SaveBMP_RW(surface, ops, 1);
	}
	else if (format == Format::JPEG)
	{
		IMG_SaveJPG_RW(surface, ops, 1, 90);
	}
	else
	{
		IMG_SavePNG_RW(surface, ops, 1);
	}

	return buffer;
}

} // namespace

DeckCard::DeckCard(SDL_Surface* surface)
    : m_surface(surface)
{
	assert(m_surface && "DeckCard must be initialised with a valid surface");
}

DeckCard::DeckCard(int width, int height)
    : m_surface(nullptr)
{
	m_surface = SDL_CreateRGBSurfaceWithFormat(SDL_SIMD_ALIGNED, width, height, 32, SDL_PIXELFORMAT_ARGB32);
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

void DeckCard::init_instance_table(lua_State* L)
{
}

int DeckCard::index(lua_State* L, std::string_view const& key) const
{
	if (key == "w" || key == "width")
	{
		lua_pushinteger(L, m_surface ? m_surface->w : 0);
	}
	else if (key == "h" || key == "height")
	{
		lua_pushinteger(L, m_surface ? m_surface->h : 0);
	}
	else
	{
		lua_pushnil(L);
	}
	return 1;
}

int DeckCard::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "w" || key == "width" || key == "h" || key == "height")
	{
		luaL_error(L, "key is readonly for DeckCard");
	}
	else
	{
		newindex_store_in_instance_table(L);
	}
	return 0;
}

int DeckCard::tostring(lua_State* L) const
{
	lua_settop(L, 1);
	lua_getfield(L, 1, "src");
	lua_getfield(L, 1, "text");

	luaL_Buffer buf;
	luaL_buffinit(L, &buf);
	lua_pushfstring(L, "%s { width=%d, height=%d", LUA_TYPENAME, m_surface->w, m_surface->h);
	luaL_addvalue(&buf);

	if (lua_type(L, 2) == LUA_TSTRING)
	{
		luaL_addstring(&buf, ", src='");
		lua_pushvalue(L, 2);
		luaL_addvalue(&buf);
		luaL_addlstring(&buf, "'", 1);
	}

	if (lua_type(L, 3) == LUA_TSTRING)
	{
		luaL_addstring(&buf, ", text='");
		lua_pushvalue(L, 3);
		luaL_addvalue(&buf);
		luaL_addlstring(&buf, "'", 1);
	}

	luaL_addstring(&buf, " }");
	luaL_pushresult(&buf);

	return 1;
}

SDL_Surface* DeckCard::resize_surface(SDL_Surface* surface, int new_width, int new_height)
{
	if (!surface)
		return nullptr;
	if (surface->w <= 0 || surface->h <= 0 || !surface->format)
		return nullptr;
	if (new_width <= 0 && new_height <= 0)
		return nullptr;

	if (new_width <= 0)
		new_width = (surface->w * new_height) / surface->h;

	if (new_height <= 0)
		new_height = (surface->h * new_width) / surface->w;

	SDL_Surface* new_surface = SDL_CreateRGBSurfaceWithFormat(SDL_SIMD_ALIGNED, new_width, new_height, 32, SDL_PIXELFORMAT_ARGB32);
	if (!new_surface)
		return nullptr;

	SDL_SetSurfaceBlendMode(new_surface, SDL_BLENDMODE_NONE);
	SDL_BlitScaled(surface, nullptr, new_surface, nullptr);
	return new_surface;
}

std::vector<unsigned char> DeckCard::save_surface_as_bmp(SDL_Surface* surface)
{
	return save_surface_as(surface, Format::BMP);
}

std::vector<unsigned char> DeckCard::save_surface_as_jpeg(SDL_Surface* surface)
{
	return save_surface_as(surface, Format::JPEG);
}

std::vector<unsigned char> DeckCard::save_surface_as_png(SDL_Surface* surface)
{
	return save_surface_as(surface, Format::PNG);
}

int DeckCard::_lua_blit(lua_State* L)
{
	DeckCard* self = from_stack(L, 1);
	DeckCard* card = from_stack(L, 2);

	SDL_Surface* source = card->get_surface();
	int x               = 0;
	int y               = 0;
	int w               = 0;
	int h               = 0;

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
		SDL_SetSurfaceBlendMode(self->m_surface, SDL_BLENDMODE_BLEND);
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
