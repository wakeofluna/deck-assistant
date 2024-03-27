#include "lua_helpers.h"
#include "deck_logger.h"
#include <cassert>
#include <iostream>
#include <string>

namespace
{

constexpr char const k_weak_key_metatable_key[]   = "deck:WeakKeyMetatable";
constexpr char const k_weak_value_metatable_key[] = "deck:WeakValueMetatable";

LuaHelpers::ErrorContext g_last_error_context;

int _lua_error_handler(lua_State* L)
{
	g_last_error_context.message = lua_tostring(L, 1);
	LuaHelpers::lua_lineinfo(L, g_last_error_context.source_name, g_last_error_context.line);
	lua_pop(L, 1);
	return 0;
}

int _lua_error_index;

} // namespace

void LuaHelpers::ErrorContext::clear()
{
	result = LUA_OK;
	message.clear();
	source_name.clear();
	line = 0;
}

int LuaHelpers::absidx(lua_State* L, int idx)
{
	return (idx < 0 ? lua_gettop(L) + idx + 1 : idx);
}

void LuaHelpers::push_standard_weak_key_metatable(lua_State* L)
{
	lua_getfield(L, LUA_REGISTRYINDEX, k_weak_key_metatable_key);
	if (lua_type(L, -1) != LUA_TTABLE)
	{
		lua_pop(L, 1);

		lua_createtable(L, 0, 2);
		lua_pushliteral(L, "k");
		lua_setfield(L, -2, "__mode");
		lua_pushstring(L, k_weak_key_metatable_key);
		lua_setfield(L, -2, "__metatable");

		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, k_weak_key_metatable_key);
	}
}

void LuaHelpers::push_standard_weak_value_metatable(lua_State* L)
{
	lua_getfield(L, LUA_REGISTRYINDEX, k_weak_value_metatable_key);
	if (lua_type(L, -1) != LUA_TTABLE)
	{
		lua_pop(L, 1);

		lua_createtable(L, 0, 2);
		lua_pushliteral(L, "v");
		lua_setfield(L, -2, "__mode");
		lua_pushstring(L, k_weak_key_metatable_key);
		lua_setfield(L, -2, "__metatable");

		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, k_weak_value_metatable_key);
	}
}

void LuaHelpers::push_class_table(lua_State* L, int idx)
{
	if (lua_type(L, idx) == LUA_TUSERDATA && lua_getmetatable(L, idx))
	{
		lua_rawgeti(L, -1, IDX_META_CLASSTABLE);
		lua_replace(L, -2);
	}
	else
	{
		lua_pushnil(L);
	}
}

void LuaHelpers::push_instance_table(lua_State* L, int idx)
{
	lua_getfenv(L, idx);
}

void LuaHelpers::push_instance_list_table(lua_State* L, int idx)
{
	if (lua_type(L, idx) == LUA_TUSERDATA && lua_getmetatable(L, idx))
	{
		lua_rawgeti(L, -1, IDX_META_INSTANCELIST);
		lua_replace(L, -2);
	}
	else
	{
		lua_pushnil(L);
	}
}

std::string_view LuaHelpers::to_string_view(lua_State* L, int idx)
{
	if (lua_type(L, idx) != LUA_TSTRING)
		return std::string_view();

	std::size_t len;
	char const* str = lua_tolstring(L, idx, &len);
	return std::string_view(str, len);
}

std::string_view LuaHelpers::check_arg_string(lua_State* L, int idx, bool allow_empty)
{
	idx = absidx(L, idx);

	if (lua_type(L, idx) != LUA_TSTRING)
		luaL_typerror(L, idx, "string");

	size_t len;
	char const* str = lua_tolstring(L, idx, &len);

	if (!allow_empty && len == 0)
		luaL_argerror(L, idx, "string value cannot be empty");

	return std::string_view(str, len);
}

std::string_view LuaHelpers::check_arg_string_or_none(lua_State* L, int idx)
{
	int const vtype = lua_type(L, idx);

	if (vtype == LUA_TNONE)
		return std::string_view();

	if (vtype == LUA_TSTRING)
	{
		size_t len;
		char const* str = lua_tolstring(L, idx, &len);
		return std::string_view(str, len);
	}

	idx = absidx(L, idx);
	luaL_typerror(L, idx, "string or none");
	return std::string_view();
}

lua_Integer LuaHelpers::check_arg_int(lua_State* L, int idx)
{
	idx = absidx(L, idx);

	if (lua_type(L, idx) != LUA_TNUMBER)
		luaL_typerror(L, idx, "integer");

	return lua_tointeger(L, idx);
}

bool LuaHelpers::check_arg_bool(lua_State* L, int idx)
{
	idx = absidx(L, idx);

	if (lua_type(L, idx) != LUA_TBOOLEAN)
		luaL_typerror(L, idx, "boolean");

	return lua_toboolean(L, idx);
}

std::string_view LuaHelpers::push_converted_to_string(lua_State* L, int idx)
{
	idx              = absidx(L, idx);
	int const oldtop = lua_gettop(L);
	int const vtype  = lua_type(L, idx);

	lua_checkstack(L, oldtop + 5);

	switch (vtype)
	{
		case LUA_TNONE:
			lua_pushliteral(L, "none");
			break;
		case LUA_TNIL:
			lua_pushliteral(L, "nil");
			break;
		case LUA_TBOOLEAN:
			if (lua_toboolean(L, idx))
				lua_pushliteral(L, "true");
			else
				lua_pushliteral(L, "false");
			break;
		case LUA_TNUMBER:
			lua_pushvalue(L, idx);
			lua_tolstring(L, -1, nullptr);
			break;
		case LUA_TSTRING:
			lua_pushvalue(L, idx);
			break;
		case LUA_TUSERDATA:
		case LUA_TTABLE:
			if (lua_getmetatable(L, idx) != 0)
			{
				lua_getfield(L, -1, "__tostring");
				if (lua_type(L, -1) == LUA_TFUNCTION)
				{
					lua_pushvalue(L, idx);
					if (lua_pcall(L, 1, 1, 0) == LUA_OK)
					{
						if (lua_type(L, -1) != LUA_TSTRING)
							push_converted_to_string(L, -1);

						lua_replace(L, oldtop + 1);
						lua_settop(L, oldtop + 1);
						break;
					}
					else
					{
						lua_pop(L, 1);
					}
				}

				if (vtype == LUA_TUSERDATA)
				{
					lua_getfield(L, -2, "__name");
					if (lua_type(L, -1) == LUA_TSTRING)
					{
						lua_pushfstring(L, "%s: %p", lua_tostring(L, -1), lua_topointer(L, idx));
						lua_replace(L, oldtop + 1);
						lua_settop(L, oldtop + 1);
						break;
					}
				}

				// Clean up all failures
				lua_settop(L, oldtop);
			}
			// fallthrough
		default:
			lua_pushfstring(L, "%s: %p", lua_typename(L, vtype), lua_topointer(L, idx));
			break;
	}

	assert(lua_gettop(L) == oldtop + 1 && "push_converted_to_string() failed to produce a single value");
	assert(lua_type(L, -1) == LUA_TSTRING && "push_converted_to_string() failed to produce a string");

	size_t len;
	char const* str = lua_tolstring(L, -1, &len);
	return std::string_view(str, len);
}

void LuaHelpers::newindex_store_in_instance_table(lua_State* L)
{
	push_instance_table(L, 1);
	lua_pushvalue(L, 2);
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	lua_pop(L, 1);
}

void LuaHelpers::copy_table_fields(lua_State* L)
{
	lua_checkstack(L, lua_gettop(L) + 4);

	// Simple table copy. Metatable will do the rest
	lua_pushnil(L);
	while (lua_next(L, -2) != 0)
	{
		lua_pushvalue(L, -2);
		lua_insert(L, -2);
		lua_settable(L, -5);
	}

	// Pop the original table, leaving only the target
	lua_pop(L, 1);
}

bool LuaHelpers::load_script(lua_State* L, char const* file_name, bool log_error)
{
	g_last_error_context.clear();
	g_last_error_context.result = luaL_loadfile(L, file_name);
	if (g_last_error_context.result != LUA_OK)
	{
		g_last_error_context.message     = lua_tostring(L, -1);
		g_last_error_context.source_name = file_name;
		lua_pop(L, 1);

		if (log_error)
			DeckLogger::log_message(L, DeckLogger::Level::Error, g_last_error_context.message);

		return false;
	}

	assign_new_env_table(L, -1, file_name);
	return true;
}

bool LuaHelpers::load_script_inline(lua_State* L, char const* chunk_name, std::string_view const& script, bool log_error)
{
	g_last_error_context.clear();
	g_last_error_context.result = luaL_loadbuffer(L, script.data(), script.size(), chunk_name);
	if (g_last_error_context.result != LUA_OK)
	{
		g_last_error_context.message     = lua_tostring(L, -1);
		g_last_error_context.source_name = chunk_name;
		lua_pop(L, 1);

		if (log_error)
			DeckLogger::log_message(L, DeckLogger::Level::Error, g_last_error_context.message);

		return false;
	}

	assign_new_env_table(L, -1, chunk_name);
	return true;
}

void LuaHelpers::assign_new_env_table(lua_State* L, int idx, char const* chunk_name)
{
	idx = absidx(L, idx);
	assert(lua_isfunction(L, idx));

	// Create a new ENV table for the new chunk
	lua_createtable(L, 0, 0);
	// Create a metatable that indexes into the original ENV table
	lua_createtable(L, 0, 3);
	lua_pushboolean(L, true);
	lua_setfield(L, -2, "__metatable");
	lua_getfenv(L, idx);
	lua_setfield(L, -2, "__index");
	lua_pushstring(L, chunk_name);
	lua_setfield(L, -2, "__name");
	// Set the metatable on the new ENV table
	lua_setmetatable(L, -2);
	// Assign the new ENV table to the function
	lua_setfenv(L, -2);
}

bool LuaHelpers::pcall(lua_State* L, int nargs, int nresults, bool log_error)
{
	bool const use_lua_error_handler = lua_tocfunction(L, _lua_error_index) == _lua_error_handler;
	int const funcidx                = lua_gettop(L) - nargs;

	g_last_error_context.clear();
	g_last_error_context.result = lua_pcall(L, nargs, nresults, use_lua_error_handler ? _lua_error_index : 0);

	if (g_last_error_context.result != LUA_OK)
	{
		if (!use_lua_error_handler)
			g_last_error_context.message = LuaHelpers::to_string_view(L, -1);

		lua_settop(L, funcidx - 1);

		if (log_error)
			DeckLogger::log_message(L, DeckLogger::Level::Error, g_last_error_context.message);

		return false;
	}

	return true;
}

bool LuaHelpers::lua_lineinfo(lua_State* L, std::string& short_src, int& currentline)
{
	lua_Debug ar;
	int depth = 1;

	while (lua_getstack(L, depth, &ar))
	{
		lua_getinfo(L, "l", &ar);
		if (ar.currentline != -1)
		{
			lua_getinfo(L, "S", &ar);
			short_src   = ar.short_src;
			currentline = ar.currentline;
			return true;
		}
		++depth;
	}

	return false;
}

void LuaHelpers::install_error_context_handler(lua_State* L)
{
	lua_pushcfunction(L, &_lua_error_handler);
	_lua_error_index = lua_gettop(L);
}

LuaHelpers::ErrorContext const& LuaHelpers::get_last_error_context()
{
	return g_last_error_context;
}

void LuaHelpers::debug_dump_stack(std::ostream& stream, lua_State* L, char const* description)
{
	stream << "===== Lua stack";
	if (description)
		stream << " - " << description;
	stream << " =====" << std::endl;

	for (int i = 1; lua_type(L, i) != LUA_TNONE; ++i)
	{
		std::string_view value_str = push_converted_to_string(L, i);
		stream << i << ": " << value_str << std::endl;
		lua_pop(L, 1);
	}
}

void LuaHelpers::debug_dump_stack(lua_State* L, char const* description)
{
	debug_dump_stack(std::cout, L, description);
}

void LuaHelpers::debug_dump_table(std::ostream& stream, lua_State* L, int idx, bool recursive, char const* description)
{
	stream << "===== Lua table";
	if (description)
		stream << " - " << description;
	stream << " =====" << std::endl;

	if (lua_type(L, idx) != LUA_TTABLE)
	{
		stream << "Index is not a table but a " << lua_typename(L, lua_type(L, idx)) << std::endl;
		return;
	}

	idx = absidx(L, idx);
	lua_pushnil(L);
	while (lua_next(L, idx))
	{
		std::string_view key   = push_converted_to_string(L, -2);
		std::string_view value = push_converted_to_string(L, -2);
		stream << key << " : " << value << std::endl;
		lua_pop(L, 3);
	}
	lua_pop(L, 1);
}

void LuaHelpers::debug_dump_table(lua_State* L, int idx, bool recursive, char const* description)
{
	debug_dump_table(std::cout, L, idx, recursive, description);
}
