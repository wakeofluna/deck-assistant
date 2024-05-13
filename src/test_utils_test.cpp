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

#include "test_utils_test.h"
#include "deck_logger.h"
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
	if (setjmp(g_panic_jmp))
	{
		std::string full_message  = "Unhandled panic: ";
		full_message             += g_panic_msg;
		FAIL(full_message);
	}

	lua_checkstack(L, 100);
	luaL_openlibs(L);

	// Disable logging by default
	DeckLogger::override_min_level(DeckLogger::Level::Error);

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

bool get_and_pop_key_value_in_table(lua_State* L, int idx)
{
	idx = LuaHelpers::absidx(L, idx);

	if (lua_type(L, idx) != LUA_TTABLE)
		return false;

	lua_pushnil(L);
	if (lua_next(L, idx) == 0)
		return false;

	lua_pushvalue(L, -2);
	lua_pushnil(L);
	lua_settable(L, idx);
	return true;
}

std::optional<int> to_int(lua_State* L, int idx)
{
	if (lua_type(L, idx) == LUA_TNUMBER)
		return lua_tointeger(L, idx);
	else
		return std::nullopt;
}
