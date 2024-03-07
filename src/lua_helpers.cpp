#include "lua_helpers.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{

constexpr char const k_instance_list_key[]        = "deck-assistant.Instances";
constexpr char const k_instance_tables_key[]      = "deck-assistant.InstanceTables";
constexpr char const k_weak_key_metatable_key[]   = "deck-assistant.WeakKeyMetatable";
constexpr char const k_weak_value_metatable_key[] = "deck-assistant.WeakValueMetatable";

LuaHelpers::ErrorContext g_last_error_context;

int _lua_error_handler(lua_State* L)
{
	g_last_error_context.message = lua_tostring(L, 1);

	lua_Debug ar;

	int level = 1;
	while (lua_getstack(L, level, &ar) == 1)
	{
		auto& frame = g_last_error_context.stack.emplace_back();

		lua_getinfo(L, "nSl", &ar);
		frame.source_name   = ar.short_src;
		frame.function_name = ar.name ? ar.name : ar.namewhat;
		frame.line          = ar.currentline;
		frame.line_defined  = ar.linedefined;

		++level;
	}

	return 0;
}

} // namespace

void LuaHelpers::ErrorContext::reset()
{
	call_result = LUA_OK;
	message.clear();
	stack.clear();
}

void LuaHelpers::push_this(lua_State* L) const
{
	lua_getfield(L, LUA_REGISTRYINDEX, k_instance_list_key);
	lua_pushlightuserdata(L, const_cast<LuaHelpers*>(this));
	lua_rawget(L, -2);
	lua_replace(L, -2);
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
		lua_pushboolean(L, true);
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
		lua_pushboolean(L, true);
		lua_setfield(L, -2, "__metatable");

		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, k_weak_value_metatable_key);
	}
}

void LuaHelpers::push_class_table_container(lua_State* L)
{
	lua_getfield(L, LUA_REGISTRYINDEX, k_instance_list_key);
	if (lua_type(L, -1) != LUA_TTABLE)
	{
		lua_pop(L, 1);
		lua_createtable(L, 0, 1024);

		push_standard_weak_value_metatable(L);
		lua_setmetatable(L, -2);

		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, k_instance_list_key);
	}
}

void LuaHelpers::push_instance_table_container(lua_State* L)
{
	lua_getfield(L, LUA_REGISTRYINDEX, k_instance_tables_key);
	if (lua_type(L, -1) != LUA_TTABLE)
	{
		lua_pop(L, 1);
		lua_createtable(L, 0, 1024);

		push_standard_weak_key_metatable(L);
		lua_setmetatable(L, -2);

		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, k_instance_tables_key);
	}
}

int LuaHelpers::push_class_table_of_top_value(lua_State* L)
{
	if (lua_type(L, -1) != LUA_TUSERDATA)
		luaL_checkudata(L, -1, "userdata");
	push_class_table(L, -1);
	return 1;
}

int LuaHelpers::push_instance_table_of_top_value(lua_State* L)
{
	if (lua_type(L, -1) != LUA_TUSERDATA)
		luaL_checkudata(L, -1, "userdata");
	push_instance_table(L, -1);
	return 1;
}

void LuaHelpers::push_class_table(lua_State* L, int idx)
{
	lua_getmetatable(L, idx);
	lua_rawgeti(L, -1, IDX_META_CLASSTABLE);
	assert(lua_type(L, -1) == LUA_TTABLE && "class has no class table!");
	lua_replace(L, -2);
}

void LuaHelpers::push_instance_table(lua_State* L, int idx)
{
	idx = absidx(L, idx);

	lua_getfield(L, LUA_REGISTRYINDEX, k_instance_tables_key);
	lua_pushvalue(L, idx);
	lua_rawget(L, -2);
	assert(lua_type(L, -1) == LUA_TTABLE && "class has no instance table!");
	lua_replace(L, -2);
}

void LuaHelpers::index_cache_in_class_table(lua_State* L)
{
	push_class_table(L, 1);
	lua_pushvalue(L, 2);
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	lua_pop(L, 1);
}

void LuaHelpers::index_cache_in_instance_table(lua_State* L)
{
	push_instance_table(L, 1);
	lua_pushvalue(L, 2);
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	lua_pop(L, 1);
}

void* LuaHelpers::check_arg_userdata(lua_State* L, int idx, char const* tname, bool throw_error)
{
	int equal = 0;

	void* p = lua_touserdata(L, idx);
	if (p != nullptr && lua_getmetatable(L, idx))
	{
		lua_getfield(L, LUA_REGISTRYINDEX, tname);
		equal = lua_rawequal(L, -1, -2);
		lua_pop(L, 2);
	}

	if (equal)
		return p;

	if (throw_error)
	{
		idx = absidx(L, idx);
		luaL_typerror(L, idx, tname);
	}

	return nullptr;
}

std::string_view LuaHelpers::check_arg_string(lua_State* L, int idx, bool allow_empty)
{
	idx = absidx(L, idx);
	luaL_checktype(L, idx, LUA_TSTRING);

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
	luaL_checktype(L, idx, LUA_TNUMBER);

	int isnum;
	lua_Integer value = lua_tointegerx(L, idx, &isnum);
	if (!isnum)
		luaL_typerror(L, idx, "integer");

	return value;
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
				}
				lua_getfield(L, -1, "__name");
				if (lua_type(L, -1) == LUA_TSTRING)
				{
					lua_pushfstring(L, "%s: %p", lua_tostring(L, -1), lua_topointer(L, idx));
					lua_replace(L, oldtop + 1);
					lua_settop(L, oldtop + 1);
					break;
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

void LuaHelpers::newindex_type_or_convert(lua_State* L, char const* tname, TypeCreateFunction type_create, char const* text_field)
{
	if (!check_arg_userdata(L, -1, tname, false))
	{
		int const vtype = lua_type(L, -1);

		assert(type_create);
		(*type_create)(L);

		lua_insert(L, -2);

		if (text_field && vtype == LUA_TSTRING)
		{
			lua_setfield(L, -2, text_field);
		}
		else if (vtype == LUA_TTABLE)
		{
			copy_table_fields(L);
		}
		else
		{
			lua_remove(L, -2);
			luaL_typerror(L, absidx(L, -1), tname);
		}
	}
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

bool LuaHelpers::load_script(lua_State* L, char const* file_name)
{
	int const top = lua_gettop(L);

	g_last_error_context.reset();
	g_last_error_context.call_result = luaL_loadfile(L, file_name);

	if (g_last_error_context.call_result != LUA_OK)
	{
		if (lua_gettop(L) > top)
			g_last_error_context.message = lua_tostring(L, -1);

		lua_settop(L, top);
		return false;
	}

	assign_new_env_table(L, -1, file_name);
	return true;
}

bool LuaHelpers::load_script_inline(lua_State* L, char const* chunk_name, std::string_view const& script)
{
	int const top = lua_gettop(L);

	g_last_error_context.reset();
	g_last_error_context.call_result = luaL_loadbuffer(L, script.data(), script.size(), chunk_name);

	if (g_last_error_context.call_result != LUA_OK)
	{
		if (lua_gettop(L) > top)
			g_last_error_context.message = lua_tostring(L, -1);

		lua_settop(L, top);
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

bool LuaHelpers::pcall(lua_State* L, int nargs, int nresults)
{
	int const funcidx = lua_gettop(L) - nargs;

	lua_pushcfunction(L, _lua_error_handler);
	lua_insert(L, funcidx);

	g_last_error_context.reset();
	g_last_error_context.call_result = lua_pcall(L, nargs, nresults, funcidx);

	lua_remove(L, funcidx);
	lua_settop(L, funcidx - 1);

	return g_last_error_context.call_result == LUA_OK;
}

LuaHelpers::ErrorContext const& LuaHelpers::get_last_error_context()
{
	return g_last_error_context;
}

void LuaHelpers::print_error_context(std::ostream& stream, ErrorContext const& context)
{
	switch (context.call_result)
	{
		case LUA_ERRRUN:
			stream << "Runtime error";
			break;
		case LUA_ERRFILE:
			stream << "Unable to open/read file";
			break;
		case LUA_ERRSYNTAX:
			stream << "Syntax error";
			break;
		case LUA_ERRMEM:
			stream << "Out of memory";
			break;
		case LUA_ERRERR:
			stream << "Internal error";
			break;
		default:
			stream << "Lua error #" << context.call_result;
			break;
	}

	stream << std::endl
	       << context.message << std::endl;

	if (context.stack.empty())
		return;

	stream << "=== Callstack ===" << std::endl;
	for (size_t i = 0; i < context.stack.size(); ++i)
	{
		auto const& frame = context.stack[i];

		stream << '#' << i << ' ';
		if (frame.line == -1)
			stream << frame.source_name;
		else
			stream << '[' << frame.source_name << ':' << frame.line << ']';

		if (i == 0 && !frame.function_name.empty())
			stream << " in function " << frame.function_name << "()";
		else if (i > 0 && !context.stack[i - 1].function_name.empty())
			stream << ' ' << context.stack[i - 1].function_name << "()";

		stream << std::endl;
	}
}

void LuaHelpers::debug_dump_stack(lua_State* L, char const* description)
{
	std::cout << "===== Lua stack";
	if (description)
		std::cout << " - " << description;
	std::cout << " =====" << std::endl;

	for (int i = 1;; ++i)
	{
		int const vtype = lua_type(L, i);

		if (vtype == LUA_TNONE)
			break;

		std::string_view value_str = push_converted_to_string(L, i);
		std::cout << i << ": " << value_str << std::endl;
		lua_pop(L, 1);
	}
}
