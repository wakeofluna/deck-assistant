#include "lua_class.h"
#include "lua_class_private.h"
#include <cassert>

void LuaClassBase::push_this(lua_State* L) const
{
	lua_getfield(L, LUA_REGISTRYINDEX, "deck-assistant.LuaClass");
	lua_pushlightuserdata(L, const_cast<LuaClassBase*>(this));
	lua_rawget(L, -2);
	lua_replace(L, -2);
}

void LuaClassBase::push_class_table(lua_State* L, int idx)
{
	lua_getmetatable(L, idx);
	int typ = lua_rawgeti(L, -1, IDX_META_CLASSTABLE);
	(void)typ;
	assert(typ == LUA_TTABLE && "class has no class table!");
	lua_replace(L, -2);
}

void LuaClassBase::push_instance_table(lua_State* L, int idx)
{
	int typ = lua_getiuservalue(L, idx, IDX_USER_INSTANCETABLE);
	(void)typ;
	assert(typ == LUA_TTABLE && "class has no instance table!");
}

void LuaClassBase::index_cache_in_class_table(lua_State* L)
{
	push_class_table(L, 1);
	lua_pushvalue(L, 2);
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	lua_pop(L, 1);
}

void LuaClassBase::index_cache_in_instance_table(lua_State* L)
{
	push_instance_table(L, 1);
	lua_pushvalue(L, 2);
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	lua_pop(L, 1);
}

std::string_view LuaClassBase::check_arg_string(lua_State* L, int idx)
{
	if (idx < 0)
		idx += lua_gettop(L) + 1;

	luaL_checktype(L, idx, LUA_TSTRING);

	size_t len;
	char const* str = lua_tolstring(L, idx, &len);

	return std::string_view(str, len);
}

lua_Integer LuaClassBase::check_arg_int(lua_State* L, int idx)
{
	if (idx < 0)
		idx += lua_gettop(L) + 1;

	luaL_checktype(L, idx, LUA_TNUMBER);

	int isnum;
	lua_Integer value = lua_tointegerx(L, idx, &isnum);

	if (!isnum)
		luaL_typeerror(L, idx, "integer");

	return value;
}
