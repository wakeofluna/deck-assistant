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

#include "deck_promise_list.h"
#include "deck_promise.h"
#include "lua_helpers.h"

char const* DeckPromiseList::LUA_TYPENAME = "deck:PromiseList";

DeckPromiseList::DeckPromiseList() noexcept
    : m_default_timeout(5000)
{
}

void DeckPromiseList::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &_lua_new_promise);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "new");
	lua_setfield(L, -2, "new_promise");

	lua_pushcfunction(L, &_lua_fulfill_promise);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "fulfill");
	lua_setfield(L, -2, "fulfill_promise");
}

void DeckPromiseList::init_instance_table(lua_State* L)
{
	LuaHelpers::push_standard_weak_value_metatable(L);
	lua_setmetatable(L, -2);
}

int DeckPromiseList::index(lua_State* L, std::string_view const& key) const
{
	if (key == "default_timeout")
	{
		lua_pushinteger(L, m_default_timeout);
	}
	return lua_gettop(L) == 2 ? 0 : 1;
}

int DeckPromiseList::newindex(lua_State* L)
{
	int const ktype = lua_type(L, 2);

	if (ktype == LUA_TSTRING)
	{
		std::string_view key = LuaHelpers::to_string_view(L, 2);

		if (key == "default_timeout")
		{
			int value = LuaHelpers::check_arg_int(L, 3);
			luaL_argcheck(L, (value > 0), 3, "default_timeout must be larger than zero");
			m_default_timeout = value;
		}
		else if (key == "all" || key == "pending")
		{
			lua_createtable(L, 0, 0);

			lua_createtable(L, 0, 2);
			lua_pushboolean(L, true);
			lua_setfield(L, -2, "__metatable");
			LuaHelpers::push_instance_table(L, 1);
			lua_setfield(L, -2, "__index");

			lua_setmetatable(L, -2);
		}
		else
		{
			if (lua_type(L, 3) != LUA_TNIL)
				DeckPromise::from_stack(L, 3);

			LuaHelpers::newindex_store_in_instance_table(L);
		}
	}
	else
	{
		if (lua_type(L, 3) != LUA_TNIL)
			DeckPromise::from_stack(L, 3);

		LuaHelpers::newindex_store_in_instance_table(L);
	}
	return 0;
}

int DeckPromiseList::tostring(lua_State* L) const
{
	lua_pushfstring(L, "%s { default_timeout=%d }", LUA_TYPENAME, m_default_timeout);
	return 1;
}

int DeckPromiseList::_lua_new_promise(lua_State* L)
{
	DeckPromiseList* self = DeckPromiseList::from_stack(L, 1);

	int const ktype = lua_type(L, 2);
	luaL_argcheck(L, (ktype != LUA_TNONE && ktype != LUA_TNIL), 2, "new promise requires a non-nil identifier");

	int timeout;
	if (int const ttype = lua_type(L, 3); ttype == LUA_TNUMBER)
		timeout = lua_tointeger(L, 3);
	else if (ttype == LUA_TNONE)
		timeout = self->m_default_timeout;
	else
		timeout = -1;
	luaL_argcheck(L, (timeout > 0), 3, "timeout must be an integer larger than zero");

	DeckPromise::push_new(L, timeout);

	// Store key in Promise
	LuaHelpers::push_instance_table(L, -1);
	lua_pushliteral(L, "key");
	lua_pushvalue(L, 2);
	lua_rawset(L, -3);
	lua_pop(L, 1);

	LuaHelpers::push_instance_table(L, 1);
	// Check that key is not already in PromiseList instance table
	lua_pushvalue(L, 2);
	lua_rawget(L, -2);
	luaL_argcheck(L, (lua_type(L, -1) == LUA_TNIL), 2, "a promise already exists with that key");
	lua_pop(L, 1);
	// Store key->promise in PromiseList instance table
	lua_pushvalue(L, 2);
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	// Get rid of the PromiseList instance table
	lua_pop(L, 1);

	return 1;
}

int DeckPromiseList::_lua_fulfill_promise(lua_State* L)
{
	DeckPromiseList::from_stack(L, 1);
	luaL_checkany(L, 2);
	luaL_checkany(L, 3);

	// Get promise from instance table
	LuaHelpers::push_instance_table(L, 1);
	lua_pushvalue(L, 2);
	lua_rawget(L, -2);

	if (DeckPromise* promise = DeckPromise::from_stack(L, -1, false); promise)
	{
		// Remove promise from instance table
		lua_pushvalue(L, -2);
		lua_pushvalue(L, 2);
		lua_pushnil(L);
		lua_rawset(L, -3);
		lua_pop(L, 1);

		// Store value into promise
		LuaHelpers::push_instance_table(L, -1);
		lua_pushliteral(L, "value");
		lua_pushvalue(L, 3);
		lua_rawset(L, -3);
		lua_pop(L, 1);

		promise->mark_as_fulfilled();
	}
	else
	{
		lua_pushnil(L);
	}

	return 1;
}
