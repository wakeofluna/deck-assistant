#include "deck_module.h"
#include "deck_card.h"
#include "deck_colour.h"
#include "deck_connector.h"
#include "deck_connector_container.h"
#include "deck_font.h"
#include "deck_logger.h"
#include "deck_rectangle.h"
#include <SDL_image.h>
#include <cassert>

char const* DeckModule::LUA_TYPENAME = "deck:DeckModule";

DeckModule::DeckModule()
    : m_last_clock(0)
    , m_last_delta(0)
{
}

void DeckModule::tick(lua_State* L, lua_Integer clock)
{
	m_last_delta = clock - m_last_clock;
	m_last_clock = clock;

	DeckConnectorContainer* container = get_connector_container(L);
	container->for_each(L, [this](lua_State* L, DeckConnector* dc) {
		dc->tick(L, m_last_delta);
	});

	lua_pop(L, 1);
}

void DeckModule::shutdown(lua_State* L)
{
	DeckConnectorContainer* container = get_connector_container(L);
	container->for_each(L, [](lua_State* L, DeckConnector* dc) {
		dc->shutdown(L);
	});

	lua_pop(L, 1);
}

bool DeckModule::is_exit_requested() const
{
	return m_exit_requested.has_value();
}

int DeckModule::get_exit_code() const
{
	return m_exit_requested.value_or(0);
}

void DeckModule::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &DeckModule::_lua_create_card);
	lua_setfield(L, -2, "Card");

	lua_pushcfunction(L, &DeckModule::_lua_create_colour);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "Colour");
	lua_setfield(L, -2, "Color");

	lua_pushcfunction(L, &DeckModule::_lua_create_font);
	lua_setfield(L, -2, "Font");

	lua_pushcfunction(L, &DeckModule::_lua_create_image);
	lua_setfield(L, -2, "Image");

	lua_pushcfunction(L, &DeckModule::_lua_create_rectangle);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "Rectangle");
	lua_setfield(L, -2, "Rect");

	lua_pushcfunction(L, &DeckModule::_lua_request_quit);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "quit");
	lua_setfield(L, -2, "exit");
}

void DeckModule::init_instance_table(lua_State* L)
{
	DeckConnectorContainer::push_new(L);
	lua_setfield(L, -2, "connectors");
}

int DeckModule::index(lua_State* L, std::string_view const& key) const
{
	if (key == "clock")
	{
		lua_pushinteger(L, m_last_clock);
	}
	else if (key == "delta")
	{
		lua_pushinteger(L, m_last_delta);
	}
	else
	{
		lua_pushnil(L);
	}
	return 1;
}

int DeckModule::newindex(lua_State* L)
{
	luaL_error(L, "%s instance is closed for modifications", type_name());
	return 0;
}

DeckConnectorContainer* DeckModule::get_connector_container(lua_State* L)
{
	assert(from_stack(L, -1, false) && "DeckModule::get_connector_container needs self on -1");
	lua_getfield(L, -1, "connectors");
	DeckConnectorContainer* container = DeckConnectorContainer::from_stack(L, -1, false);
	assert(container && "DeckModule lost its connector container");
	return container;
}

int DeckModule::_lua_create_card(lua_State* L)
{
	from_stack(L, 1);
	int width  = check_arg_int(L, 2);
	int height = check_arg_int(L, 3);

	luaL_argcheck(L, width > 0, 2, "width must be larger than 0");
	luaL_argcheck(L, height > 0, 3, "height must be larger than 0");

	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(SDL_SIMD_ALIGNED, width, height, 32, SDL_PIXELFORMAT_ARGB32);
	if (!surface)
	{
		DeckLogger::lua_log_message(L, DeckLogger::Level::Error, "failed to allocate new card", SDL_GetError());
		return 0;
	}

	// Fill with transparant black
	SDL_FillRect(surface, nullptr, 0);

	DeckCard::push_new(L, surface);
	return 1;
}

int DeckModule::_lua_create_colour(lua_State* L)
{
	from_stack(L, 1);
	lua_settop(L, 2);

	int const vtype = lua_type(L, 2);
	if (vtype == LUA_TSTRING)
	{
		std::string_view value = check_arg_string(L, 2);

		Colour col;
		if (!Colour::parse_colour(value, col))
		{
			DeckLogger::lua_log_message(L, DeckLogger::Level::Warning, "color value not understood");
			col.set_pink();
		}

		DeckColour::push_new(L, col);
		return 1;
	}
	else if (vtype == LUA_TTABLE)
	{
		DeckColour::push_new(L, Colour(0, 0, 0));
		lua_pushvalue(L, 2);
		LuaHelpers::copy_table_fields(L);
		return 1;
	}
	else
	{
		luaL_typerror(L, 2, "string or table");
		return 0;
	}
}

int DeckModule::_lua_create_font(lua_State* L)
{
	from_stack(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);

	DeckFont::push_new(L);
	lua_pushvalue(L, 2);
	LuaHelpers::copy_table_fields(L);

	return 1;
}

int DeckModule::_lua_create_image(lua_State* L)
{
	from_stack(L, 1);
	std::string_view src = check_arg_string(L, 2);

	SDL_Surface* tmp_surface = IMG_Load(src.data());
	if (!tmp_surface)
	{
		DeckLogger::lua_log_message(L, DeckLogger::Level::Error, "failed to load image", SDL_GetError());
		return 0;
	}

	SDL_Surface* new_surface = SDL_ConvertSurfaceFormat(tmp_surface, SDL_PIXELFORMAT_ARGB32, SDL_SIMD_ALIGNED);
	SDL_FreeSurface(tmp_surface);

	if (!new_surface)
	{
		DeckLogger::lua_log_message(L, DeckLogger::Level::Error, "failed to convert image to rgb", SDL_GetError());
		return 0;
	}

	DeckCard::push_new(L, new_surface);

	// Store the src string for the user
	lua_pushvalue(L, 2);
	lua_setfield(L, -2, "src");

	return 1;
}

int DeckModule::_lua_create_rectangle(lua_State* L)
{
	from_stack(L, 1);

	if (lua_type(L, 2) == LUA_TTABLE)
	{
		DeckRectangle::push_new(L);
		lua_pushvalue(L, 2);
		copy_table_fields(L);
		return 1;
	}

	int x, y, w, h;
	if (lua_gettop(L) == 5)
	{
		x = check_arg_int(L, 2);
		y = check_arg_int(L, 3);
		w = check_arg_int(L, 4);
		h = check_arg_int(L, 5);
		luaL_argcheck(L, (w >= 0), 4, "WIDTH value must be zero or positive");
		luaL_argcheck(L, (h >= 0), 5, "HEIGHT value must be zero or positive");
	}
	else if (lua_gettop(L) == 3)
	{
		x = 0;
		y = 0;
		w = check_arg_int(L, 2);
		h = check_arg_int(L, 3);
		luaL_argcheck(L, (w >= 0), 2, "WIDTH value must be zero or positive");
		luaL_argcheck(L, (h >= 0), 3, "HEIGHT value must be zero or positive");
	}
	else
	{
		x = 0;
		y = 0;
		w = 0;
		h = 0;
	}

	DeckRectangle::push_new(L, x, y, w, h);
	return 1;
}

int DeckModule::_lua_request_quit(lua_State* L)
{
	DeckModule* self       = from_stack(L, 1);
	self->m_exit_requested = lua_tointeger(L, 2);
	return 0;
}
