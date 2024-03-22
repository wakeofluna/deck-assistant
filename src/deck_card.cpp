#include "deck_card.h"
#include "deck_colour.h"
#include "deck_logger.h"
#include "deck_rectangle.h"
#include "lua_helpers.h"
#include <SDL_image.h>
#include <algorithm>
#include <array>
#include <cassert>

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
    , m_parent_surface(nullptr)
{
	assert(m_surface && "DeckCard must be initialised with a valid surface");
}

DeckCard::DeckCard(SDL_Surface* surface, SDL_Surface* parent_surface)
    : m_surface(surface)
    , m_parent_surface(parent_surface)
{
	assert(m_surface && "DeckCard must be initialised with a valid surface");
	assert(m_parent_surface && "DeckCard must be initialised with a valid parent_surface");
	assert(m_surface->format == m_parent_surface->format && "DeckCard surface and parent_surface can't possible be related");

	unsigned char const* surface_pixels_start = reinterpret_cast<unsigned char const*>(m_surface->pixels);
	unsigned char const* surface_pixels_end   = surface_pixels_start + (m_surface->h - 1) * m_surface->pitch + m_surface->w * m_surface->format->BytesPerPixel;
	unsigned char const* parent_pixels_start  = reinterpret_cast<unsigned char const*>(m_parent_surface->pixels);
	unsigned char const* parent_pixels_end    = parent_pixels_start + (m_parent_surface->h - 1) * m_parent_surface->pitch + m_parent_surface->w * m_parent_surface->format->BytesPerPixel;
	assert(surface_pixels_start >= parent_pixels_start && surface_pixels_end <= parent_pixels_end && "DeckCard surface must be contained within the parent_surface");

	// Avoid warnings in some builds
	(void)surface_pixels_start;
	(void)surface_pixels_end;
	(void)parent_pixels_start;
	(void)parent_pixels_end;

	++m_parent_surface->refcount;
}

DeckCard::~DeckCard()
{
	if (m_surface)
		SDL_FreeSurface(m_surface);
	if (m_parent_surface)
		SDL_FreeSurface(m_parent_surface);
}

void DeckCard::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &_lua_blit);
	lua_setfield(L, -2, "blit");

	lua_pushcfunction(L, &_lua_centered);
	lua_setfield(L, -2, "centered");

	lua_pushcfunction(L, &_lua_clear);
	lua_setfield(L, -2, "clear");

	lua_pushcfunction(L, &_lua_resize);
	lua_setfield(L, -2, "resize");

	lua_pushcfunction(L, &_lua_subcard);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "sub_area");
	lua_setfield(L, -2, "sub_card");
}

void DeckCard::init_instance_table(lua_State* L)
{
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
	else if (key == "dup")
	{
		m_surface->refcount++;
		push_new(L, m_surface);
	}
	else if (key == "rect" || key == "rectangle")
	{
		DeckRectangle::push_new(L, m_surface->w, m_surface->h);
	}
	else
	{
		lua_pushnil(L);
	}
	return 1;
}

int DeckCard::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "w" || key == "width" || key == "h" || key == "height" || key == "rect" || key == "rectangle")
	{
		luaL_error(L, "key %s is readonly for %s", key.data(), LUA_TYPENAME);
	}
	else
	{
		LuaHelpers::newindex_store_in_instance_table(L);
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

	SDL_Surface* new_surface = SDL_CreateRGBSurfaceWithFormat(0, new_width, new_height, 32, SDL_PIXELFORMAT_ARGB8888);
	if (!new_surface)
		return nullptr;

	SDL_BlendMode old_blend_mode;
	SDL_GetSurfaceBlendMode(surface, &old_blend_mode);
	SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
	SDL_BlitScaled(surface, nullptr, new_surface, nullptr);
	SDL_SetSurfaceBlendMode(surface, old_blend_mode);
	SDL_SetSurfaceBlendMode(new_surface, old_blend_mode);

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
	SDL_Rect dstrect { 0, 0, source->w, source->h };

	DeckRectangle* rect = DeckRectangle::from_stack(L, 3, false);
	if (rect)
	{
		dstrect = rect->get_rectangle();
	}
	else if (lua_gettop(L) >= 4)
	{
		luaL_argcheck(L, lua_type(L, 3) == LUA_TNUMBER, 3, "X coordinate must be an integer");
		luaL_argcheck(L, lua_type(L, 4) == LUA_TNUMBER, 4, "Y coordinate must be an integer");
		dstrect.x = lua_tointeger(L, 3);
		dstrect.y = lua_tointeger(L, 4);

		if (lua_gettop(L) >= 6)
		{
			luaL_argcheck(L, lua_type(L, 5) == LUA_TNUMBER, 5, "WIDTH must be an integer");
			luaL_argcheck(L, lua_type(L, 6) == LUA_TNUMBER, 6, "HEIGHT must be an integer");
			dstrect.w = lua_tointeger(L, 5);
			dstrect.h = lua_tointeger(L, 6);
			luaL_argcheck(L, (dstrect.w > 0), 5, "WIDTH must be larger than zero");
			luaL_argcheck(L, (dstrect.h > 0), 6, "HEIGHT must be larger than zero");
		}
	}

	SDL_Rect const surface_rect = SDL_Rect { 0, 0, self->m_surface->w, self->m_surface->h };
	SDL_Rect const target_rect  = DeckRectangle::clip(surface_rect, dstrect);

	if (target_rect.w > 0 && target_rect.h > 0)
		SDL_BlitScaled(source, nullptr, self->m_surface, &dstrect);

	DeckRectangle::push_new(L, target_rect);
	return 1;
}

int DeckCard::_lua_centered(lua_State* L)
{
	DeckCard* self = from_stack(L, 1);

	if (DeckRectangle* other_rect = DeckRectangle::from_stack(L, 2, false); other_rect)
	{
		SDL_Rect self_rect { 0, 0, self->m_surface->w, self->m_surface->h };
		SDL_Rect result = DeckRectangle::centered(self_rect, other_rect->get_rectangle());
		DeckRectangle::push_new(L, result);
	}
	else
	{
		DeckCard* other_card = from_stack(L, 2);
		SDL_Rect self_rect { 0, 0, self->m_surface->w, self->m_surface->h };
		SDL_Rect card_rect { 0, 0, other_card->m_surface->w, other_card->m_surface->h };
		SDL_Rect result = DeckRectangle::centered(self_rect, card_rect);
		DeckRectangle::push_new(L, result);
	}
	return 1;
}

int DeckCard::_lua_clear(lua_State* L)
{
	DeckCard* self     = from_stack(L, 1);
	DeckColour* colour = DeckColour::from_stack(L, 2);

	SDL_Color color = colour->get_colour();
	SDL_FillRect(self->m_surface, nullptr, SDL_MapRGBA(self->m_surface->format, color.r, color.g, color.b, color.a));

	// When the surface is fully opaque we don't need alpha blending
	SDL_BlendMode blend_mode = (color.a != 255) ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE;
	SDL_SetSurfaceBlendMode(self->m_surface, blend_mode);

	lua_settop(L, 1);
	return 1;
}

int DeckCard::_lua_resize(lua_State* L)
{
	DeckCard* self = from_stack(L, 1);
	int new_width  = LuaHelpers::check_arg_int(L, 2);
	int new_height = LuaHelpers::check_arg_int(L, 3);
	luaL_argcheck(L, (new_width > 0), 2, "WIDTH must be larger than zero");
	luaL_argcheck(L, (new_height > 0), 3, "HEIGHT must be larger than zero");

	if (new_width != self->m_surface->w || new_height != self->m_surface->h)
	{
		SDL_Surface* new_surface = resize_surface(self->m_surface, new_width, new_height);
		if (!new_surface)
		{
			DeckLogger::lua_log_message(L, DeckLogger::Level::Warning, "deck:Card resize failed");
		}
		else
		{
			SDL_FreeSurface(self->m_surface);
			self->m_surface = new_surface;
		}
	}

	lua_settop(L, 1);
	return 1;
}

int DeckCard::_lua_subcard(lua_State* L)
{
	DeckCard* self = from_stack(L, 1);
	SDL_Rect self_rect { 0, 0, self->m_surface->w, self->m_surface->h };
	SDL_Rect sub_rect;

	if (lua_gettop(L) == 5)
	{
		sub_rect.x = LuaHelpers::check_arg_int(L, 2);
		sub_rect.y = LuaHelpers::check_arg_int(L, 3);
		sub_rect.w = LuaHelpers::check_arg_int(L, 4);
		sub_rect.h = LuaHelpers::check_arg_int(L, 5);
	}
	else
	{
		DeckRectangle* rect = DeckRectangle::from_stack(L, 2);
		sub_rect            = rect->get_rectangle();
	}

	SDL_Rect clip_rect = DeckRectangle::clip(self_rect, sub_rect);
	if (clip_rect.w <= 0 || clip_rect.h <= 0)
	{
		luaL_error(L, "provided area is not within the card dimensions");
		return 0;
	}

	unsigned char const* surface_pixels  = reinterpret_cast<unsigned char const*>(self->m_surface->pixels);
	surface_pixels                      += clip_rect.y * self->m_surface->pitch + clip_rect.x * self->m_surface->format->BytesPerPixel;

	SDL_Surface* new_surface = SDL_CreateRGBSurfaceWithFormatFrom((void*)surface_pixels, clip_rect.w, clip_rect.h, 0, self->m_surface->pitch, self->m_surface->format->format);
	if (!new_surface)
	{
		DeckLogger::lua_log_message(L, DeckLogger::Level::Warning, "deck:Card creation of subcard failed");
		return 0;
	}

	DeckCard::push_new(L, new_surface, self->m_parent_surface ? self->m_parent_surface : self->m_surface);

	// Store the master card for the users reference
	LuaHelpers::push_instance_table(L, -1);
	lua_pushvalue(L, -2);
	lua_setfield(L, -2, "master");
	lua_pop(L, 1);

	return 1;
}
