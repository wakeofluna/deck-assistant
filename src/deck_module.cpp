/*
 * DeckAssistant - Creating control panels using scripts.
 * Copyright (C) 2024  Esther Dalhuisen (Wake of Luna)
 *
 * DeckAssistant is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DeckAssistant is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "deck_module.h"
#include "deck_card.h"
#include "deck_colour.h"
#include "deck_connector_container.h"
#include "deck_connector_factory.h"
#include "deck_font.h"
#include "deck_logger.h"
#include "deck_promise_list.h"
#include "deck_rectangle.h"
#include "deck_rectangle_list.h"
#include "lua_helpers.h"
#include <SDL_image.h>
#include <cassert>

namespace
{

constexpr char const* g_connector_container_name = "connectors";
constexpr char const* g_connector_factory_name   = "connector_factory";
constexpr int const g_connector_container_idx    = 1;

} // namespace

char const* DeckModule::LUA_TYPENAME = "deck:DeckModule";

DeckModule::DeckModule()
    : m_last_clock(0)
    , m_last_delta(0)
{
}

lua_Integer DeckModule::get_clock(lua_State* L)
{
	DeckModule* self = push_global_instance(L);
	lua_pop(L, 1);
	return self ? self->m_last_clock : 0;
}

void DeckModule::tick_inputs(lua_State* L, lua_Integer clock)
{
	assert(from_stack(L, -1, false) != nullptr);

	m_last_delta = clock - m_last_clock;
	m_last_clock = clock;

	LuaHelpers::push_instance_table(L, -1);
	lua_rawgeti(L, -1, g_connector_container_idx);
	lua_replace(L, -2);

	lua_pushinteger(L, clock);
	DeckConnectorContainer::for_each(L, "tick_inputs", 1);

	lua_pop(L, 2);
}

void DeckModule::tick_outputs(lua_State* L)
{
	assert(from_stack(L, -1, false) != nullptr);

	LuaHelpers::push_instance_table(L, -1);
	lua_rawgeti(L, -1, g_connector_container_idx);
	lua_replace(L, -2);

	DeckConnectorContainer::for_each(L, "tick_outputs", 0);

	lua_pop(L, 1);
}

void DeckModule::shutdown(lua_State* L)
{
	assert(from_stack(L, -1, false) != nullptr);

	LuaHelpers::push_instance_table(L, -1);
	lua_rawgeti(L, -1, g_connector_container_idx);
	lua_replace(L, -2);

	DeckConnectorContainer::for_each(L, "shutdown", 0);

	lua_pop(L, 1);
}

void DeckModule::set_exit_requested(int exit_code)
{
	if (!m_exit_requested.has_value())
		m_exit_requested = exit_code;
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

	lua_pushcfunction(L, &DeckModule::_lua_create_connector);
	lua_setfield(L, -2, "Connector");

	lua_pushcfunction(L, &DeckModule::_lua_create_font);
	lua_setfield(L, -2, "Font");

	lua_pushcfunction(L, &DeckModule::_lua_create_image);
	lua_setfield(L, -2, "Image");

	lua_pushcfunction(L, &DeckModule::_lua_create_promise_list);
	lua_setfield(L, -2, "PromiseList");

	lua_pushcfunction(L, &DeckModule::_lua_create_rectangle);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "Rectangle");
	lua_setfield(L, -2, "Rect");

	lua_pushcfunction(L, &DeckModule::_lua_create_rectangle_list);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "RectangleList");
	lua_setfield(L, -2, "RectList");

	lua_pushcfunction(L, &DeckModule::_lua_request_quit);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "quit");
	lua_setfield(L, -2, "exit");

	DeckConnectorContainer::push_new(L);
	lua_setfield(L, -2, g_connector_container_name);

	DeckConnectorFactory::push_new(L);
	lua_setfield(L, -2, g_connector_factory_name);
}

void DeckModule::init_instance_table(lua_State* L)
{
	// Copy the connector container to the instance table for fast access
	LuaHelpers::push_class_table(L, -2);
	lua_getfield(L, -1, g_connector_container_name);
	lua_replace(L, -2);
	assert(DeckConnectorContainer::from_stack(L, -1, false) != nullptr);
	lua_rawseti(L, -2, g_connector_container_idx);
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

int DeckModule::_lua_create_card(lua_State* L)
{
	from_stack(L, 1);
	int width  = LuaHelpers::check_arg_int(L, 2);
	int height = LuaHelpers::check_arg_int(L, 3);

	luaL_argcheck(L, width > 0, 2, "width must be larger than 0");
	luaL_argcheck(L, height > 0, 3, "height must be larger than 0");

	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
	if (!surface)
	{
		DeckLogger::lua_log_message(L, DeckLogger::Level::Error, "failed to allocate new surface: ", SDL_GetError());
		return 0;
	}

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
		std::string_view value = LuaHelpers::to_string_view(L, 2);

		util::Colour col;
		if (!util::Colour::parse_colour(value, col))
		{
			DeckLogger::lua_log_message(L, DeckLogger::Level::Warning, "color value not understood");
			col.set_pink();
		}

		DeckColour::push_new(L, col);
		return 1;
	}
	else if (vtype == LUA_TTABLE)
	{
		DeckColour::push_new(L, util::Colour(0, 0, 0));
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

int DeckModule::_lua_create_connector(lua_State* L)
{
	from_stack(L, 1);
	luaL_checktype(L, 2, LUA_TSTRING); // class/factory name

	if (lua_type(L, 3) != LUA_TSTRING)
	{
		lua_pushvalue(L, 2);
		lua_insert(L, 3);
	}

	luaL_checktype(L, 3, LUA_TSTRING); // connector name
	bool const has_table = lua_type(L, 4) == LUA_TTABLE;

	int const extra_args_top = lua_gettop(L);

	LuaHelpers::push_instance_table(L, 1);
	lua_rawgeti(L, -1, g_connector_container_idx);
	lua_replace(L, -2);

	// Stack:
	// userdata: DeckModule
	// string: clz
	// string: name
	// (optional args)
	// userdata: DeckConnectorContainer

	// Check that the connector does not already exist
	lua_pushvalue(L, 3);
	lua_gettable(L, -2);
	if (lua_type(L, -1) == LUA_TNIL)
	{
		// Does not exist
		lua_pop(L, 1);

		// Get the factory function from the factory
		lua_getfield(L, 1, g_connector_factory_name);
		lua_pushvalue(L, 2);
		lua_gettable(L, -2);
		lua_replace(L, -2);

		if (lua_type(L, -1) != LUA_TFUNCTION)
		{
			luaL_argerror(L, 2, "no constructor function for connector class");
			return 0;
		}

		// Run the factory function with all arguments
		for (int i = 2; i <= extra_args_top; ++i)
			lua_pushvalue(L, i);

		if (lua_pcall(L, extra_args_top - 1, 1, 0) != LUA_OK)
		{
			lua_pushfstring(L, "connector construction failed: %s", lua_tostring(L, -1));
			lua_error(L);
			return 0;
		}

		if (lua_type(L, -1) == LUA_TNIL)
		{
			luaL_error(L, "factory function failed to provide a valid return object");
			return 0;
		}

		// Store the name for the convenience of the user
		lua_pushvalue(L, 3);
		lua_setfield(L, -2, "name");

		// Insert the object into the container
		// This may also throw an error if the type is not good
		lua_pushvalue(L, -2);
		lua_pushvalue(L, 3);
		lua_pushvalue(L, -3);
		lua_settable(L, -3);
		lua_pop(L, 1);
	}

	// Now apply the table argument to the object (either old or new)
	if (has_table)
	{
		lua_pushvalue(L, 4);
		LuaHelpers::copy_table_fields(L);
	}

	// Finally done!
	return 1;
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
	std::string_view src = LuaHelpers::check_arg_string(L, 2);

	SDL_Surface* new_surface = IMG_Load(src.data());
	if (!new_surface)
	{
		DeckLogger::lua_log_message(L, DeckLogger::Level::Error, "failed to load image: ", SDL_GetError());
		return 0;
	}

	// Optimization for blitting
	if (new_surface->format->format != SDL_PIXELFORMAT_ARGB8888)
	{
		SDL_Surface* tmp_surface = SDL_ConvertSurfaceFormat(new_surface, SDL_PIXELFORMAT_ARGB8888, 0);
		if (!tmp_surface)
		{
			DeckLogger::lua_log_message(L, DeckLogger::Level::Warning, "failed to optimize image to ARGB: ", SDL_GetError());
		}
		else
		{
			bool const has_alpha = new_surface->format->Amask;

			SDL_FreeSurface(new_surface);
			new_surface = tmp_surface;

			SDL_BlendMode blend_mode = has_alpha ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE;
			SDL_SetSurfaceBlendMode(new_surface, blend_mode);
		}
	}

	DeckCard::push_new(L, new_surface);

	// Store the src string for the user
	lua_pushvalue(L, 2);
	lua_setfield(L, -2, "src");

	return 1;
}

int DeckModule::_lua_create_promise_list(lua_State* L)
{
	from_stack(L, 1);
	DeckPromiseList::push_new(L);
	return 1;
}

int DeckModule::_lua_create_rectangle(lua_State* L)
{
	from_stack(L, 1);

	if (lua_type(L, 2) == LUA_TTABLE)
	{
		DeckRectangle::push_new(L);
		lua_pushvalue(L, 2);
		LuaHelpers::copy_table_fields(L);
		return 1;
	}

	int x, y, w, h;
	if (lua_gettop(L) == 5)
	{
		x = LuaHelpers::check_arg_int(L, 2);
		y = LuaHelpers::check_arg_int(L, 3);
		w = LuaHelpers::check_arg_int(L, 4);
		h = LuaHelpers::check_arg_int(L, 5);
		luaL_argcheck(L, (w >= 0), 4, "WIDTH value must be zero or positive");
		luaL_argcheck(L, (h >= 0), 5, "HEIGHT value must be zero or positive");
	}
	else if (lua_gettop(L) == 3)
	{
		x = 0;
		y = 0;
		w = LuaHelpers::check_arg_int(L, 2);
		h = LuaHelpers::check_arg_int(L, 3);
		luaL_argcheck(L, (w >= 0), 2, "WIDTH value must be zero or positive");
		luaL_argcheck(L, (h >= 0), 3, "HEIGHT value must be zero or positive");
	}
	else
	{
		if (lua_gettop(L) != 1)
			luaL_error(L, "incorrect number of arguments (expected 0, 2 or 4)");

		x = 0;
		y = 0;
		w = 0;
		h = 0;
	}

	DeckRectangle::push_new(L, x, y, w, h);
	return 1;
}

int DeckModule::_lua_create_rectangle_list(lua_State* L)
{
	from_stack(L, 1);
	DeckRectangleList::push_new(L);
	return 1;
}

int DeckModule::_lua_request_quit(lua_State* L)
{
	DeckModule* self = from_stack(L, 1);
	self->set_exit_requested(lua_tointeger(L, 2));
	return 0;
}
