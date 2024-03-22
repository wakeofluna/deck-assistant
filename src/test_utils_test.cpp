#include "test_utils_test.h"
#include "lua_helpers.h"
#include <cassert>
#include <catch2/catch_test_macros.hpp>
#include <lua.hpp>

std::jmp_buf g_panic_jmp;
std::string g_panic_msg;

int at_panic(lua_State* L)
{
	size_t len;
	char const* str = lua_tolstring(L, -1, &len);
	g_panic_msg.assign(str, len);
	std::longjmp(g_panic_jmp, 1);
}

lua_State* new_test_state()
{
	lua_State* L = luaL_newstate();
	lua_atpanic(L, &at_panic);
	lua_checkstack(L, 100);
	if (setjmp(g_panic_jmp))
	{
		std::string full_message  = "Unhandled panic: ";
		full_message             += g_panic_msg;
		FAIL(full_message);
	}
	return L;
}

int push_dummy_value(lua_State* L, int tp)
{
	switch (tp)
	{
		case LUA_TNONE:
			return lua_gettop(L) + 6;
		case LUA_TNIL:
			lua_pushnil(L);
			break;
		case LUA_TBOOLEAN:
			lua_pushboolean(L, true);
			break;
		case LUA_TLIGHTUSERDATA:
			lua_pushlightuserdata(L, &tp);
			break;
		case LUA_TNUMBER:
			lua_pushinteger(L, 1337);
			break;
		case LUA_TSTRING:
			lua_pushfstring(L, "hello world %d", 2);
			break;
		case LUA_TTABLE:
			lua_createtable(L, 2, 2);
			break;
		case LUA_TFUNCTION:
			lua_pushcfunction(L, &at_panic);
			break;
		case LUA_TUSERDATA:
			lua_newuserdata(L, 4);
			break;
		case LUA_TTHREAD:
			lua_newthread(L);
			break;
		default:
			assert(false && "Test internal error, lua_type missing in switch statement");
			break;
	}
	return lua_gettop(L);
}

TableCountMap count_elements_in_table(lua_State* L, int idx)
{
	idx = LuaHelpers::absidx(L, idx);

	TableCountMap count_map;
	if (lua_type(L, idx) == LUA_TTABLE)
	{
		lua_pushnil(L);
		while (lua_next(L, idx) != 0)
		{
			int const tp_key = lua_type(L, -2);
			int const tp_val = lua_type(L, -1);
			count_map[tp_key].first++;
			count_map[tp_val].second++;
			lua_pop(L, 1);
		}
	}

	return count_map;
}

std::vector<std::string_view> split_string(std::string_view const& text, char split_char)
{
	std::vector<std::string_view> result;

	std::size_t offset = 0;
	while (offset < text.size())
	{
		std::size_t next = text.find(split_char, offset);
		if (next == std::string_view::npos)
		{
			result.push_back(text.substr(offset));
			break;
		}
		else
		{
			result.push_back(text.substr(offset, next - offset));
			offset = next + 1;
		}
	}

	return result;
}

std::optional<int> to_int(lua_State* L, int idx)
{
	if (lua_type(L, idx) == LUA_TNUMBER)
		return lua_tointeger(L, idx);
	else
		return std::nullopt;
}
