#include "deck_image.h"
#include "deck_logger.h"
#include "lua_class.hpp"
#include <SDL_image.h>
#include <algorithm>
#include <fstream>

template class LuaClass<DeckImage>;

namespace
{

enum class Format : char
{
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

	if (format == Format::PNG)
	{
		IMG_SavePNG_RW(surface, ops, 1);
	}
	else
	{
		IMG_SaveJPG_RW(surface, ops, 1, 90);
	}

	return buffer;
}

} // namespace

char const* DeckImage::LUA_TYPENAME = "deck:Image";

DeckImage::DeckImage()
    : m_surface(nullptr)
{
}

DeckImage::~DeckImage()
{
	if (m_surface)
		SDL_FreeSurface(m_surface);
}

int DeckImage::index(lua_State* L, std::string_view const& key) const
{
	if (key == "src")
	{
		if (!m_src.empty())
			lua_pushlstring(L, m_src.data(), m_src.size());
	}
	else if (key == "w" || key == "width")
	{
		if (m_surface)
			lua_pushinteger(L, m_surface->w);
	}
	else if (key == "h" || key == "height")
	{
		if (m_surface)
			lua_pushinteger(L, m_surface->h);
	}
	else
	{
		luaL_error(L, "invalid key for DeckImage (allowed: src, width, height)");
	}
	return lua_gettop(L) == 2 ? 0 : 1;
}

int DeckImage::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "src" || key == "__init")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, -1);
		if (m_src != value)
		{
			free_surface();
			m_src = value;
			load_surface(L);
		}
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

SDL_Surface* DeckImage::get_surface() const
{
	return m_surface;
}

bool DeckImage::load_surface(lua_State* L)
{
	if (m_surface != nullptr)
		return true;

	if (m_src.empty())
		return false;

	SDL_Surface* tmp_surface = IMG_Load(m_src.c_str());
	if (!tmp_surface)
	{
		DeckLogger::lua_log_message(L, DeckLogger::Level::Error, "failed to load image", SDL_GetError());
	}
	else
	{
		m_surface = SDL_ConvertSurfaceFormat(tmp_surface, SDL_PIXELFORMAT_ARGB32, SDL_SIMD_ALIGNED);
		SDL_FreeSurface(tmp_surface);

		if (!m_surface)
			DeckLogger::lua_log_message(L, DeckLogger::Level::Error, "failed to convert image to rgb", SDL_GetError());
	}

	return m_surface;
}

void DeckImage::free_surface()
{
	if (m_surface)
	{
		SDL_FreeSurface(m_surface);
		m_surface = nullptr;
	}
}

SDL_Surface* DeckImage::resize_surface(SDL_Surface* surface, int new_width, int new_height)
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

	SDL_BlitScaled(surface, nullptr, new_surface, nullptr);
	return new_surface;
}

std::vector<unsigned char> DeckImage::save_surface_as_png(SDL_Surface* surface)
{
	return save_surface_as(surface, Format::PNG);
}

std::vector<unsigned char> DeckImage::save_surface_as_jpeg(SDL_Surface* surface)
{
	return save_surface_as(surface, Format::JPEG);
}
