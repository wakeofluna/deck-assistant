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

#include "deck_connector_container.h"
#include "lua_helpers.h"
#include <cassert>

char const* DeckConnectorContainer::LUA_TYPENAME = "deck:ConnectorContainer";

void DeckConnectorContainer::for_each(lua_State* L, char const* function_name, int nargs)
{
	assert(nargs >= 0);
	int const arg_end   = lua_gettop(L);
	int const arg_start = arg_end - nargs + 1;
	assert(from_stack(L, arg_start - 1, false) != nullptr);

	// For repeated access
	lua_pushstring(L, function_name);

	LuaHelpers::push_instance_table(L, arg_start - 1);
	lua_pushnil(L);
	while (lua_next(L, -2))
	{
		lua_pushvalue(L, -4);
		lua_gettable(L, -2);
		if (lua_type(L, -1) == LUA_TFUNCTION)
		{
			lua_pushvalue(L, -2);
			for (int i = arg_start; i <= arg_end; ++i)
				lua_pushvalue(L, i);

			LuaHelpers::pcall(L, nargs + 1, 0);
		}
		else
		{
			// Pop the not-a-function
			lua_pop(L, 1);
		}
		// Pop the lua_next value for the next iteration
		lua_pop(L, 1);
	}
	// Pop the instance table and the function name
	lua_pop(L, 2);
}

void DeckConnectorContainer::init_instance_table(lua_State* L)
{
	LuaHelpers::push_standard_weak_value_metatable(L);
	lua_setmetatable(L, -2);
}

int DeckConnectorContainer::newindex(lua_State* L)
{
	from_stack(L, 1);
	luaL_checktype(L, 2, LUA_TSTRING);

	// Check validity of the connector
	for (char const* const key : { "tick_inputs", "tick_outputs", "shutdown" })
	{
		lua_getfield(L, 3, key);
		if (lua_type(L, -1) != LUA_TFUNCTION)
			luaL_error(L, "Candidate connector does not have a function called \"%s\"", key);
		lua_pop(L, 1);
	}

	LuaHelpers::newindex_store_in_instance_table(L);
	return 0;
}

int DeckConnectorContainer::tostring(lua_State* L) const
{
	lua_pushfstring(L, "%s: %p", LUA_TYPENAME, this);
	return 1;
}
