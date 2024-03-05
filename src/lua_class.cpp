#include "lua_class.h"
#include <cassert>
#include <iostream>

namespace
{

constexpr char const k_instance_list_key[]        = "deck-assistant.Instances";
constexpr char const k_instance_tables_key[]      = "deck-assistant.InstanceTables";
constexpr char const k_weak_key_metatable_key[]   = "deck-assistant.WeakKeyMetatable";
constexpr char const k_weak_value_metatable_key[] = "deck-assistant.WeakValueMetatable";

} // namespace

void LuaClassBase::push_this(lua_State* L) const
{
	lua_getfield(L, LUA_REGISTRYINDEX, k_instance_list_key);
	lua_pushlightuserdata(L, const_cast<LuaClassBase*>(this));
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
	int const oldtop = lua_gettop(L);
	idx              = absidx(L, idx);

	if (lua_type(L, idx) == LUA_TSTRING)
	{
		lua_pushvalue(L, idx);
	}
	else if (lua_type(L, idx) == LUA_TNUMBER)
	{
		lua_pushvalue(L, idx);
		lua_tolstring(L, -1, nullptr);
	}
	else if (luaL_callmeta(L, idx, "__tostring"))
	{
	}
	else
	{
		lua_getglobal(L, "tostring");
		lua_pushvalue(L, idx);
		lua_pcall(L, 1, 1, 0);
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
