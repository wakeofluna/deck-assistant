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

#include "deck_enum.h"
#include "lua_helpers.h"
#include <cassert>

char const* DeckEnum::LUA_TYPENAME = "deck:Enum";

DeckEnum::DeckEnum(std::string_view const& clz, std::string_view const& name, std::size_t value)
    : m_class(clz)
    , m_name(name)
    , m_value(value)
{
}

DeckEnum* DeckEnum::get_or_create(lua_State* L, std::string_view const& enum_class, std::string_view const& value_name, std::size_t value)
{
	lua_checkstack(L, 8);

	push_metatable(L);
	lua_pushlstring(L, enum_class.data(), enum_class.size());
	lua_pushvalue(L, -1);
	lua_rawget(L, -3);
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_createtable(L, 8, 0);
		lua_pushvalue(L, -1);
		lua_insert(L, -3);
		lua_rawset(L, -4);
	}
	else
	{
		lua_remove(L, -2);
	}

	lua_rawgeti(L, -1, value + 1);

	DeckEnum* deck_enum = DeckEnum::from_stack(L, -1, false);
	if (!deck_enum)
	{
		deck_enum = DeckEnum::push_new(L, enum_class, value_name, value);
		lua_rawseti(L, -3, value + 1);
	}

	lua_pop(L, 3);
	return deck_enum;
}

std::optional<int> DeckEnum::to_int(lua_State* L, int idx, std::string_view const& enum_class, bool throw_error)
{
	DeckEnum* instance = from_stack(L, idx, throw_error);
	if (!instance || instance->m_class != enum_class)
	{
		if (throw_error)
			luaL_argerror(L, LuaHelpers::absidx(L, idx), "enum of wrong class");

		return std::nullopt;
	}

	return std::make_optional(int(instance->m_value));
}

int DeckEnum::index(lua_State* L, std::string_view const& key) const
{
	if (key == "class" || key == "group")
	{
		lua_pushlstring(L, m_class.data(), m_class.size());
	}
	else if (key == "code" || key == "name")
	{
		lua_pushlstring(L, m_name.data(), m_name.size());
	}
	else if (key == "value")
	{
		lua_pushinteger(L, m_value);
	}
	return lua_gettop(L) == 2 ? 0 : 1;
}

int DeckEnum::newindex(lua_State* L)
{
	luaL_error(L, "%s instance is closed for modifications", LUA_TYPENAME);
	return 0;
}

int DeckEnum::tostring(lua_State* L) const
{
	luaL_Buffer buf;
	luaL_buffinit(L, &buf);
	luaL_addlstring(&buf, m_class.data(), m_class.size());
	luaL_addchar(&buf, '.');
	luaL_addlstring(&buf, m_name.data(), m_name.size());
	luaL_pushresult(&buf);
	return 1;
}
