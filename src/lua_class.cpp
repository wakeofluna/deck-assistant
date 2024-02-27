#include "lua_class.h"
#include "lua_class_private.h"
#include <cassert>

void LuaClassBase::push_this(lua_State* L) const
{
	lua_getfield(L, LUA_REGISTRYINDEX, k_instance_list_key);
	lua_pushlightuserdata(L, const_cast<LuaClassBase*>(this));
	lua_rawget(L, -2);
	lua_replace(L, -2);
}

int LuaHelpers::push_class_table_container(lua_State* L)
{
	lua_getfield(L, LUA_REGISTRYINDEX, k_instance_list_key);
	if (lua_type(L, -1) != LUA_TTABLE)
	{
		lua_pop(L, 1);
		lua_createtable(L, 0, 1024);

		lua_createtable(L, 0, 2);
		lua_pushliteral(L, "v");
		lua_setfield(L, -2, "__mode");
		lua_pushboolean(L, true);
		lua_setfield(L, -2, "__metatable");
		lua_setmetatable(L, -2);

		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, k_instance_list_key);
	}
	return 1;
}

int LuaHelpers::push_instance_table_container(lua_State* L)
{
	lua_getfield(L, LUA_REGISTRYINDEX, k_instance_tables_key);
	if (lua_type(L, -1) != LUA_TTABLE)
	{
		lua_pop(L, 1);
		lua_createtable(L, 0, 1024);

		lua_createtable(L, 0, 2);
		lua_pushliteral(L, "k");
		lua_setfield(L, -2, "__mode");
		lua_pushboolean(L, true);
		lua_setfield(L, -2, "__metatable");
		lua_setmetatable(L, -2);

		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, k_instance_tables_key);
	}
	return 1;
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
	int const absidx = (idx < 0 ? lua_gettop(L) + idx + 1 : idx);

	lua_getfield(L, LUA_REGISTRYINDEX, k_instance_tables_key);
	lua_pushvalue(L, absidx);
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

std::string_view LuaHelpers::check_arg_string(lua_State* L, int idx)
{
	if (idx < 0)
		idx += lua_gettop(L) + 1;

	luaL_checktype(L, idx, LUA_TSTRING);

	size_t len;
	char const* str = lua_tolstring(L, idx, &len);

	return std::string_view(str, len);
}

lua_Integer LuaHelpers::check_arg_int(lua_State* L, int idx)
{
	if (idx < 0)
		idx += lua_gettop(L) + 1;

	luaL_checktype(L, idx, LUA_TNUMBER);

	int isnum;
	lua_Integer value = lua_tointegerx(L, idx, &isnum);

	if (!isnum)
		luaL_typerror(L, idx, "integer");

	return value;
}

void LuaHelpers::push_converted_to_string(lua_State* L, int idx)
{
	if (lua_type(L, idx) == LUA_TSTRING)
	{
		lua_pushvalue(L, idx);
		return;
	}

	if (lua_type(L, idx) == LUA_TNUMBER)
	{
		lua_pushvalue(L, idx);
		lua_tolstring(L, -1, nullptr);
		return;
	}

	if (luaL_callmeta(L, idx, "__tostring"))
		return;

	if (idx < 0)
		idx += lua_gettop(L) + 1;

	lua_getglobal(L, "tostring");
	lua_pushvalue(L, idx);
	lua_pcall(L, 1, 1, 0);
}
