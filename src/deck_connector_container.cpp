#include "deck_connector_container.h"
#include "deck_logger.h"
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

			if (lua_pcall(L, nargs + 1, 0, 0) != LUA_OK)
			{
				DeckLogger::log_message(L, DeckLogger::Level::Error, "Error during Connector \"", LuaHelpers::to_string_view(L, -3), "\" function \"", function_name, "\": ", LuaHelpers::to_string_view(L, -1));
				// Pop the error message
				lua_pop(L, 1);
			}
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
