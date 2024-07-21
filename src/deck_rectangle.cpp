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

#include "deck_rectangle.h"
#include "lua_helpers.h"

char const* DeckRectangle::LUA_TYPENAME = "deck:Rectangle";

DeckRectangle::DeckRectangle()
    : m_rectangle {}
{
}

DeckRectangle::DeckRectangle(SDL_Rect const& rect)
    : m_rectangle(rect)
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

void DeckRectangle::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &_lua_centered);
	lua_setfield(L, -2, "centered");

	lua_pushcfunction(L, &_lua_contains);
	lua_setfield(L, -2, "contains");

	lua_pushcfunction(L, &_lua_set_size);
	lua_setfield(L, -2, "set_size");

	lua_pushcfunction(L, &_lua_set_position);
	lua_setfield(L, -2, "set_position");

	lua_pushcfunction(L, &_lua_move);
	lua_setfield(L, -2, "move");

	lua_pushcfunction(L, &_lua_clip);
	lua_setfield(L, -2, "clip");

	lua_pushcfunction(L, &_lua_reset);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "clear");
	lua_setfield(L, -2, "reset");
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
	else if (key == "right")
	{
		lua_pushinteger(L, m_rectangle.x + m_rectangle.w);
	}
	else if (key == "bottom")
	{
		lua_pushinteger(L, m_rectangle.y + m_rectangle.h);
	}
	else if (key == "valid")
	{
		lua_pushboolean(L, m_rectangle.w > 0 && m_rectangle.h > 0);
	}
	else if (key == "dup")
	{
		DeckRectangle* dup = push_new(L);
		dup->m_rectangle   = m_rectangle;
	}
	else
	{
		lua_pushnil(L);
	}
	return 1;
}

int DeckRectangle::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "x" || key == "left")
	{
		int value     = LuaHelpers::check_arg_int(L, 3);
		m_rectangle.x = value;
	}
	else if (key == "y" || key == "top")
	{
		int value     = LuaHelpers::check_arg_int(L, 3);
		m_rectangle.y = value;
	}
	else if (key == "w" || key == "width")
	{
		int value = LuaHelpers::check_arg_int(L, 3);
		luaL_argcheck(L, (value >= 0), 3, "value must be positive");
		m_rectangle.w = value;
	}
	else if (key == "h" || key == "height")
	{
		int value = LuaHelpers::check_arg_int(L, 3);
		luaL_argcheck(L, (value >= 0), 3, "value must be positive");
		m_rectangle.h = value;
	}
	else if (key == "right")
	{
		int value = LuaHelpers::check_arg_int(L, 3);
		luaL_argcheck(L, (value >= m_rectangle.x), 3, "right value must be larger than left coordinate");
		m_rectangle.w = value - m_rectangle.x;
	}
	else if (key == "bottom")
	{
		int value = LuaHelpers::check_arg_int(L, 3);
		luaL_argcheck(L, (value >= m_rectangle.y), 3, "bottom value must be larger than top coordinate");
		m_rectangle.h = value - m_rectangle.y;
	}
	else if (key == "valid" || key == "dup")
	{
		luaL_error(L, "key %s is readonly for %s", key.data(), LUA_TYPENAME);
	}
	else
	{
		LuaHelpers::newindex_store_in_instance_table(L);
	}
	return 0;
}

int DeckRectangle::tostring(lua_State* L) const
{
	lua_pushfstring(L, "%s { x=%d, y=%d, w=%d, h=%d }", LUA_TYPENAME, int(m_rectangle.x), int(m_rectangle.y), int(m_rectangle.w), int(m_rectangle.h));
	return 1;
}

SDL_Rect DeckRectangle::centered(SDL_Rect const& object, SDL_Rect const& frame)
{
	SDL_Rect rect;
	rect.x = frame.x + (frame.w - object.w) / 2;
	rect.y = frame.y + (frame.h - object.h) / 2;
	rect.w = object.w;
	rect.h = object.h;
	return rect;
}

SDL_Rect DeckRectangle::clip(SDL_Rect const& lhs, SDL_Rect const& rhs)
{
	SDL_Rect rect;

	int const left   = std::max(lhs.x, rhs.x);
	int const top    = std::max(lhs.y, rhs.y);
	int const right  = std::min(lhs.x + lhs.w, rhs.x + rhs.w);
	int const bottom = std::min(lhs.y + lhs.h, rhs.y + rhs.h);

	if (left < right)
	{
		rect.x = left;
		rect.w = right - left;
	}
	else
	{
		rect.w = 0;
		rect.x = lhs.x;
		if (rect.x < right)
			rect.x += lhs.w;
	}

	if (top < bottom)
	{
		rect.y = top;
		rect.h = bottom - top;
	}
	else
	{
		rect.h = 0;
		rect.y = lhs.y;
		if (rect.y < bottom)
			rect.y += lhs.h;
	}

	return rect;
}

bool DeckRectangle::contains(SDL_Rect const& rect, int x, int y)
{
	return (x >= rect.x
	        && y >= rect.y
	        && x < rect.x + rect.w
	        && y < rect.y + rect.h);
}

int DeckRectangle::_lua_centered(lua_State* L)
{
	DeckRectangle* self  = from_stack(L, 1);
	DeckRectangle* other = from_stack(L, 2);

	SDL_Rect rect     = DeckRectangle::centered(self->m_rectangle, other->m_rectangle);
	self->m_rectangle = rect;

	lua_settop(L, 1);
	return 1;
}

int DeckRectangle::_lua_contains(lua_State* L)
{
	DeckRectangle* self = from_stack(L, 1);
	int x               = LuaHelpers::check_arg_int(L, 2);
	int y               = LuaHelpers::check_arg_int(L, 3);

	bool result = contains(self->m_rectangle, x, y);

	lua_pushboolean(L, result);
	return 1;
}

int DeckRectangle::_lua_set_size(lua_State* L)
{
	DeckRectangle* self = from_stack(L, 1);
	int w               = LuaHelpers::check_arg_int(L, 2);
	int h               = LuaHelpers::check_arg_int(L, 3);

	luaL_argcheck(L, (w >= 0), 2, "WIDTH must be positive");
	luaL_argcheck(L, (h >= 0), 3, "HEIGHT must be positive");

	self->m_rectangle.w = w;
	self->m_rectangle.h = h;

	lua_settop(L, 1);
	return 1;
}

int DeckRectangle::_lua_set_position(lua_State* L)
{
	DeckRectangle* self = from_stack(L, 1);
	int x               = LuaHelpers::check_arg_int(L, 2);
	int y               = LuaHelpers::check_arg_int(L, 3);

	self->m_rectangle.x = x;
	self->m_rectangle.y = y;

	lua_settop(L, 1);
	return 1;
}

int DeckRectangle::_lua_move(lua_State* L)
{
	DeckRectangle* self = from_stack(L, 1);
	int x               = LuaHelpers::check_arg_int(L, 2);
	int y               = LuaHelpers::check_arg_int(L, 3);

	self->m_rectangle.x += x;
	self->m_rectangle.y += y;

	lua_settop(L, 1);
	return 1;
}

int DeckRectangle::_lua_clip(lua_State* L)
{
	DeckRectangle* self        = from_stack(L, 1);
	DeckRectangle const* other = from_stack(L, 2);

	SDL_Rect rect     = clip(self->m_rectangle, other->m_rectangle);
	self->m_rectangle = rect;

	lua_settop(L, 1);
	return 1;
}

int DeckRectangle::_lua_reset(lua_State* L)
{
	DeckRectangle* self = from_stack(L, 1);

	self->m_rectangle.x = 0;
	self->m_rectangle.y = 0;
	self->m_rectangle.w = 0;
	self->m_rectangle.h = 0;

	lua_settop(L, 1);
	return 1;
}
