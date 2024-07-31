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

DeckPromiseList::DeckPromiseList(int default_timeout) noexcept
    : m_default_timeout(default_timeout)
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

int DeckPromiseList::new_promise(lua_State* L, int timeout) const
{
	// Required stack:
	// -2 = PromiseList
	// -1 = Key

	// Stack out on success:
	// -2 = PromiseList
	// -1 = Promise

	// Stack out on failure
	// -1 = PromiseList

	if (timeout < 0)
		timeout = m_default_timeout;

	DeckPromise::push_new(L, timeout);

	// Store key in Promise
	LuaHelpers::push_instance_table(L, -1);
	lua_pushliteral(L, "key");
	lua_pushvalue(L, -4);
	lua_rawset(L, -3);
	lua_pop(L, 1);

	LuaHelpers::push_instance_table(L, -3);
	// Check that key is not already in PromiseList instance table
	lua_pushvalue(L, -3);
	lua_rawget(L, -2);
	if (!lua_isnil(L, -1))
	{
		// Promise with that key already exists
		lua_pop(L, 4);
		return 0;
	}
	lua_pop(L, 1);

	// Store key->promise in PromiseList instance table
	lua_pushvalue(L, -3);
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	// Get rid of the PromiseList instance table
	lua_pop(L, 1);
	// Get rid of the key
	lua_replace(L, -2);

	return 1;
}

int DeckPromiseList::fulfill_promise(lua_State* L) const
{
	// Required stack:
	// -3 = PromiseList
	// -2 = Key
	// -1 = Value

	// Stack out on success:
	// -2 = PromiseList
	// -1 = Promise

	// Stack out on failure
	// -1 = PromiseList

	// Get promise from instance table
	LuaHelpers::push_instance_table(L, -3);
	lua_pushvalue(L, -3);
	lua_rawget(L, -2);

	if (DeckPromise* promise = DeckPromise::from_stack(L, -1, false); promise)
	{
		// Remove promise from instance table
		lua_pushvalue(L, -2);
		lua_pushvalue(L, -5);
		lua_pushnil(L);
		lua_rawset(L, -3);
		lua_pop(L, 1);

		// Store value into promise
		LuaHelpers::push_instance_table(L, -1);
		lua_pushliteral(L, "value");
		lua_pushvalue(L, -5);
		lua_rawset(L, -3);
		lua_pop(L, 1);

		promise->mark_as_fulfilled();

		lua_replace(L, -4);
		lua_pop(L, 2);
		return 1;
	}
	else
	{
		lua_pop(L, 4);
		return 0;
	}
}

int DeckPromiseList::fulfill_all_promises(lua_State* L) const
{
	// Required stack:
	// -2 = PromiseList
	// -1 = Value

	// Stack out:
	// -1 = PromiseList

	int promises_done = 0;

	LuaHelpers::push_instance_table(L, -2);
	lua_pushnil(L);
	while (lua_next(L, -2))
	{
		if (DeckPromise* promise = DeckPromise::from_stack(L, -1, false); promise)
		{
			// Store value into promise
			LuaHelpers::push_instance_table(L, -1);
			lua_pushliteral(L, "value");
			lua_pushvalue(L, -6);
			lua_rawset(L, -3);
			lua_pop(L, 1);

			promise->mark_as_fulfilled();

			// Delete promise from table
			lua_pushvalue(L, -2);
			lua_pushnil(L);
			lua_rawset(L, -5);

			++promises_done;
		}

		lua_pop(L, 1);
	}
	lua_pop(L, 2);

	return promises_done;
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

	lua_settop(L, 2);

	int result = self->new_promise(L);
	if (result == 0)
		luaL_argerror(L, 2, "promise with that key already exists");

	return result;
}

int DeckPromiseList::_lua_fulfill_promise(lua_State* L)
{
	DeckPromiseList* self = DeckPromiseList::from_stack(L, 1);
	luaL_checkany(L, 2);
	luaL_checkany(L, 3);

	lua_settop(L, 3);
	return self->fulfill_promise(L);
}
