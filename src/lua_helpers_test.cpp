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

#include "lua_class.h"
#include "lua_helpers.h"
#include "test_utils_test.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <csetjmp>
#include <string_view>

namespace lua_helpers_test
{

class TestClass : public LuaClass<TestClass>
{
public:
	int tostring(lua_State* L) const
	{
		lua_pushfstring(L, "It's a secret (%d)", 333);
		return 1;
	}
};

class TestClass2 : public LuaClass<TestClass2>
{
public:
	static char const* LUA_TYPENAME;

	TestClass2()
	    : m_initial_value(0)
	{
	}

	TestClass2(int initial_value)
	    : m_initial_value(initial_value)
	{
	}

	static void init_class_table(lua_State* L)
	{
		lua_pushliteral(L, "TestClass2");
		lua_setfield(L, -2, "table");
	}

	void init_instance_table(lua_State* L)
	{
		lua_pushliteral(L, "INSTANCE TABLE!");
		lua_setfield(L, -2, "table");

		lua_pushinteger(L, m_initial_value);
		lua_setfield(L, -2, "value");
	}

	int m_initial_value;
};

char const* TestClass2::LUA_TYPENAME = "TestClass2Type";

} // namespace lua_helpers_test

using namespace lua_helpers_test;

#include "lua_class.hpp"
template class LuaClass<TestClass>;
template class LuaClass<TestClass2>;

TEST_CASE("LuaHelpers", "[lua]")
{
	lua_State* L = new_test_state();

	TestClass::push_new(L);
	lua_setfield(L, LUA_REGISTRYINDEX, "tc1");

	TestClass2::push_new(L, 42);
	lua_setfield(L, LUA_REGISTRYINDEX, "tc2");

	TestClass2::push_new(L, 69);
	lua_setfield(L, LUA_REGISTRYINDEX, "tc3");

	REQUIRE(lua_gettop(L) == 0);

	SECTION("absidx")
	{
		int const max_idx = 33;

		for (int i = 0; i < max_idx; ++i)
			lua_pushinteger(L, 1);
		REQUIRE(lua_gettop(L) == max_idx);

		for (int i = 1; i < max_idx; ++i)
		{
			REQUIRE(LuaHelpers::absidx(L, i) == i);
			REQUIRE(LuaHelpers::absidx(L, -i) == max_idx + 1 - i);
		}
	}

	SECTION("push_standard_weak_key_metatable")
	{
		LuaHelpers::push_standard_weak_key_metatable(L);
		LuaHelpers::push_standard_weak_key_metatable(L);
		LuaHelpers::push_standard_weak_key_metatable(L);

		REQUIRE(lua_gettop(L) == 3);
		REQUIRE(lua_rawequal(L, 1, 2));
		REQUIRE(lua_rawequal(L, 2, 3));

		lua_getfield(L, 1, "__metatable");
		REQUIRE(lua_toboolean(L, -1) == true);

		lua_getfield(L, 1, "__mode");
		REQUIRE(LuaHelpers::to_string_view(L, -1) == "k");
	}

	SECTION("push_standard_weak_value_metatable")
	{
		LuaHelpers::push_standard_weak_value_metatable(L);
		LuaHelpers::push_standard_weak_value_metatable(L);
		LuaHelpers::push_standard_weak_value_metatable(L);

		REQUIRE(lua_gettop(L) == 3);
		REQUIRE(lua_rawequal(L, 1, 2));
		REQUIRE(lua_rawequal(L, 2, 3));

		lua_getfield(L, 1, "__metatable");
		REQUIRE(lua_toboolean(L, -1) == true);

		lua_getfield(L, 1, "__mode");
		REQUIRE(LuaHelpers::to_string_view(L, -1) == "v");
	}

	SECTION("push_class_table")
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "tc1");

		LuaHelpers::push_class_table(L, 1);
		REQUIRE(lua_isnil(L, -1));
		lua_settop(L, 1);

		lua_getfield(L, LUA_REGISTRYINDEX, "tc2");
		lua_getfield(L, LUA_REGISTRYINDEX, "tc3");
		LuaHelpers::push_class_table(L, 2);
		LuaHelpers::push_class_table(L, 3);
		REQUIRE(lua_gettop(L) == 5);
		REQUIRE(lua_rawequal(L, -2, -1));

		lua_getfield(L, 4, "table");
		REQUIRE(LuaHelpers::to_string_view(L, -1) == "TestClass2");
		lua_getfield(L, 4, "value");
		REQUIRE(lua_isnil(L, -1));
	}

	SECTION("push_instance_table")
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "tc1");

		LuaHelpers::push_instance_table(L, 1);
		REQUIRE(lua_type(L, -1) == LUA_TTABLE);
		lua_settop(L, 1);

		lua_getfield(L, LUA_REGISTRYINDEX, "tc2");
		lua_getfield(L, LUA_REGISTRYINDEX, "tc3");
		LuaHelpers::push_instance_table(L, 2);
		LuaHelpers::push_instance_table(L, 3);
		REQUIRE(lua_gettop(L) == 5);
		REQUIRE(!lua_rawequal(L, -2, -1));

		lua_getfield(L, 4, "table");
		REQUIRE(LuaHelpers::to_string_view(L, -1) == "INSTANCE TABLE!");
		lua_getfield(L, 5, "table");
		REQUIRE(LuaHelpers::to_string_view(L, -1) == "INSTANCE TABLE!");
		lua_getfield(L, 4, "value");
		REQUIRE(lua_tointeger(L, -1) == 42);
		lua_getfield(L, 5, "value");
		REQUIRE(lua_tointeger(L, -1) == 69);
	}

	SECTION("check_arg_string")
	{
		for (int tp = LUA_TNONE; tp <= LUA_TTHREAD; ++tp)
		{
			if (tp == LUA_TSTRING || tp == LUA_TNONE)
				continue;

			DYNAMIC_SECTION("Failure test for type " << tp << ':' << lua_typename(L, tp))
			{
				int idx = push_dummy_value(L, tp);
				EXPECT_ERROR(LuaHelpers::check_arg_string(L, idx));
				REQUIRE_THAT(g_panic_msg, Catch::Matchers::StartsWith("bad argument"));

				idx = push_dummy_value(L, tp);
				EXPECT_ERROR(LuaHelpers::check_arg_string_or_none(L, idx));
				REQUIRE_THAT(g_panic_msg, Catch::Matchers::StartsWith("bad argument"));
			}
		}

		SECTION("None")
		{
			REQUIRE(LuaHelpers::check_arg_string_or_none(L, 5) == "");
			REQUIRE(lua_gettop(L) == 0);
			EXPECT_ERROR(LuaHelpers::check_arg_string(L, 5));
			REQUIRE_THAT(g_panic_msg, Catch::Matchers::StartsWith("bad argument"));
		}

		SECTION("Valid string")
		{
			lua_pushfstring(L, "hello world %d", 2);
			REQUIRE(LuaHelpers::check_arg_string(L, 1) == "hello world 2");
			REQUIRE(LuaHelpers::check_arg_string_or_none(L, 1) == "hello world 2");
			REQUIRE(lua_gettop(L) == 1);
		}

		SECTION("Empty string")
		{
			lua_pushliteral(L, "");
			REQUIRE(LuaHelpers::check_arg_string(L, 1, true) == "");
			REQUIRE(LuaHelpers::check_arg_string_or_none(L, 1) == "");
			REQUIRE(lua_gettop(L) == 1);
			EXPECT_ERROR(LuaHelpers::check_arg_string(L, 1));
			REQUIRE_THAT(g_panic_msg, Catch::Matchers::StartsWith("bad argument"));
		}
	}

	SECTION("check_arg_int")
	{
		for (int tp = LUA_TNONE; tp <= LUA_TTHREAD; ++tp)
		{
			if (tp == LUA_TNUMBER)
				continue;

			DYNAMIC_SECTION("Failure test for type " << tp << ':' << lua_typename(L, tp))
			{
				int idx = push_dummy_value(L, tp);
				EXPECT_ERROR(LuaHelpers::check_arg_int(L, idx));
				REQUIRE_THAT(g_panic_msg, Catch::Matchers::StartsWith("bad argument"));
			}
		}

		SECTION("Success test integer")
		{
			lua_pushinteger(L, 123);
			REQUIRE(LuaHelpers::check_arg_int(L, 1) == 123);
			REQUIRE(lua_gettop(L) == 1);
		}

		SECTION("Success test float")
		{
			// Lua 5.1 doesn't differentiate between ints and floats and just truncates
			lua_pushnumber(L, 45.9);
			REQUIRE(LuaHelpers::check_arg_int(L, 1) == 45);
			REQUIRE(lua_gettop(L) == 1);
		}
	}

	SECTION("push_converted_to_string")
	{
		std::string_view converted;

		for (int tp = LUA_TNONE; tp <= LUA_TTHREAD; ++tp)
		{
			DYNAMIC_SECTION("String conversion for type " << tp << ':' << lua_typename(L, tp))
			{
				switch (tp)
				{
					case LUA_TNONE:
						converted = LuaHelpers::push_converted_to_string(L, 5);
						REQUIRE(converted == "none");
						break;

					case LUA_TNIL:
						lua_pushnil(L);
						converted = LuaHelpers::push_converted_to_string(L, 1);
						REQUIRE(converted == "nil");
						break;

					case LUA_TBOOLEAN:
						lua_pushboolean(L, true);
						converted = LuaHelpers::push_converted_to_string(L, 1);
						REQUIRE(converted == "true");
						lua_pop(L, 2);

						lua_pushboolean(L, false);
						converted = LuaHelpers::push_converted_to_string(L, 1);
						REQUIRE(converted == "false");
						break;

					case LUA_TLIGHTUSERDATA:
						lua_pushlightuserdata(L, &tp);
						converted = LuaHelpers::push_converted_to_string(L, 1);
						REQUIRE(converted.starts_with("userdata: "));
						break;

					case LUA_TNUMBER:
						lua_pushinteger(L, 1337);
						converted = LuaHelpers::push_converted_to_string(L, 1);
						REQUIRE(converted.starts_with("1337"));
						break;

					case LUA_TSTRING:
						lua_pushfstring(L, "hello world %d", 2);
						converted = LuaHelpers::push_converted_to_string(L, 1);
						REQUIRE(converted.starts_with("hello world 2"));
						REQUIRE(lua_rawequal(L, 1, 2));
						break;

					case LUA_TTABLE:
						lua_createtable(L, 2, 2);
						converted = LuaHelpers::push_converted_to_string(L, 1);
						REQUIRE(converted.starts_with("table: "));
						break;

					case LUA_TFUNCTION:
						lua_pushcfunction(L, &at_panic);
						converted = LuaHelpers::push_converted_to_string(L, 1);
						REQUIRE(converted.starts_with("function: "));
						break;

					case LUA_TUSERDATA:
						lua_newuserdata(L, 4);
						converted = LuaHelpers::push_converted_to_string(L, 1);
						REQUIRE(converted.starts_with("userdata: "));
						break;

					case LUA_TTHREAD:
						lua_newthread(L);
						converted = LuaHelpers::push_converted_to_string(L, 1);
						REQUIRE(converted.starts_with("thread: "));
						break;

					default:
						FAIL("Test internal error, lua_type missing in switch statement");
						break;
				}

				if (tp == LUA_TNONE)
				{
					REQUIRE(lua_gettop(L) == 1);
				}
				else
				{
					// Make sure no type coercion took place
					REQUIRE(lua_type(L, 1) == tp);
					REQUIRE(lua_gettop(L) == 2);
				}
			}
		}

		SECTION("Userdata with __name")
		{
			lua_getfield(L, LUA_REGISTRYINDEX, "tc2");
			converted = LuaHelpers::push_converted_to_string(L, 1);
			REQUIRE(lua_gettop(L) == 2);
			std::string class_name = std::string(TestClass2::LUA_TYPENAME) + ": ";
			REQUIRE_THAT(std::string(converted), Catch::Matchers::StartsWith(class_name));
		}

		SECTION("Userdata with __tostring")
		{
			lua_getfield(L, LUA_REGISTRYINDEX, "tc1");
			converted = LuaHelpers::push_converted_to_string(L, 1);
			REQUIRE(lua_gettop(L) == 2);
			REQUIRE(converted == "It's a secret (333)");
		}
	}

	SECTION("newindex_store_in_instance_table")
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "tc1");
		lua_pushliteral(L, "value");
		lua_pushinteger(L, 1);
		LuaHelpers::newindex_store_in_instance_table(L);
		REQUIRE(lua_gettop(L) == 3);
		lua_settop(L, 0);

		lua_getfield(L, LUA_REGISTRYINDEX, "tc2");

		lua_pushliteral(L, "value");
		lua_gettable(L, -2);
		REQUIRE(lua_tointeger(L, -1) == 42);
		lua_pop(L, 1);

		lua_pushliteral(L, "value");
		lua_pushinteger(L, 1337);
		REQUIRE(lua_gettop(L) == 3);
		LuaHelpers::newindex_store_in_instance_table(L);
		REQUIRE(lua_gettop(L) == 3);
		lua_pop(L, 2);

		lua_pushliteral(L, "value");
		lua_gettable(L, -2);
		REQUIRE(lua_tointeger(L, -1) == 1337);
		lua_pop(L, 1);
	}

	SECTION("copy_table_fields")
	{
		lua_createtable(L, 0, 0);
		lua_createtable(L, 0, 0);
		REQUIRE(!lua_rawequal(L, 1, 2));

		// Table 1
		lua_pushinteger(L, 11);
		lua_setfield(L, 1, "yours");
		//
		lua_pushstring(L, "test");
		lua_rawseti(L, 1, 1);
		//
		lua_getfield(L, LUA_REGISTRYINDEX, "tc1");
		lua_rawseti(L, 1, 2);
		//
		lua_createtable(L, 0, 0);
		lua_pushliteral(L, "this is a table");
		lua_settable(L, 1);

		// Table 2
		lua_pushinteger(L, 55);
		lua_setfield(L, 2, "mine");
		lua_pushinteger(L, 33);
		lua_setfield(L, 2, "yours");

		REQUIRE(lua_gettop(L) == 2);

		// Now copy table 1 into table 2
		lua_pushvalue(L, 1);
		LuaHelpers::copy_table_fields(L);

		REQUIRE(lua_gettop(L) == 2);
		REQUIRE(!lua_rawequal(L, 1, 2));

		int num_keys = 0;

		lua_pushnil(L);
		while (lua_next(L, 2) == 1)
		{
			++num_keys;

			lua_pushvalue(L, -2);
			lua_gettable(L, 1);

			if (LuaHelpers::to_string_view(L, -3) == "mine")
			{
				REQUIRE(lua_tointeger(L, -2) == 55);
				REQUIRE(lua_isnil(L, -1));
			}
			else
			{
				REQUIRE(lua_rawequal(L, -1, -2));
			}

			lua_pop(L, 2);
		}

		REQUIRE(num_keys == 5);
	}

	SECTION("load_script")
	{
		SUCCEED("Not running test that requires filesystem access");
	}

	SECTION("load_script_inline")
	{
		SECTION("Valid script")
		{
			std::string_view const script = R"str(function foo(a)
	return a + 2
end
value = foo(15)
)str";

			REQUIRE(LuaHelpers::load_script_inline(L, "inline_test_chunk", script));
			REQUIRE(lua_gettop(L) == 1);

			lua_pushvalue(L, 1);
			REQUIRE(lua_pcall(L, 0, 0, 0) == LUA_OK);

			lua_getfenv(L, 1);
			lua_pushvalue(L, LUA_GLOBALSINDEX);
			REQUIRE(!lua_rawequal(L, -2, -1));

			lua_getfield(L, 2, "value");
			REQUIRE(lua_tointeger(L, -1) == 17);
			lua_pop(L, 1);

			lua_getfield(L, 3, "value");
			REQUIRE(lua_isnil(L, -1));
			lua_pop(L, 1);
		}

		SECTION("Script with syntax error")
		{
			std::string_view const script = "a = 1\nb = 2\nc = 3 = 4\nd = 5";

			REQUIRE(!LuaHelpers::load_script_inline(L, "inline_test_chunk", script, false));
			REQUIRE(lua_gettop(L) == 0);

			LuaHelpers::ErrorContext const& error = LuaHelpers::get_last_error_context();
			REQUIRE(error.message.find("inline_test_chunk") != std::string::npos);
			REQUIRE(error.message.find(":3:") != std::string::npos);
		}
	}

	SECTION("pcall")
	{
		LuaHelpers::install_error_context_handler(L);

		SECTION("C-function that works correctly")
		{
			auto func = [](lua_State* L) -> int {
				lua_Integer n = LuaHelpers::check_arg_int(L, 1);
				lua_pushinteger(L, n + 2);
				return 1;
			};

			lua_pushcfunction(L, func);
			lua_pushinteger(L, 9);
			REQUIRE(LuaHelpers::pcall(L, 1, 1, false));

			REQUIRE(lua_gettop(L) == 2);
			REQUIRE(lua_tointeger(L, 2) == 11);
			lua_pop(L, 1);
		}

		SECTION("C-function that throws a lua error")
		{
			auto func = [](lua_State* L) -> int {
				lua_pushnil(L);
				luaL_error(L, "I can't let this happen");
				return 1;
			};

			lua_pushcfunction(L, func);
			lua_pushnil(L);
			REQUIRE(!LuaHelpers::pcall(L, 1, 1, false));

			LuaHelpers::ErrorContext const& error = LuaHelpers::get_last_error_context();
			REQUIRE(error.result == LUA_ERRRUN);
			REQUIRE(error.message == "I can't let this happen");
			REQUIRE(error.source_name.empty());
			REQUIRE(error.line == 0);
		}

		SECTION("Attempt to call a table")
		{
			lua_createtable(L, 0, 0);
			lua_pushinteger(L, 2);

			REQUIRE(!LuaHelpers::pcall(L, 1, 2, false));

			LuaHelpers::ErrorContext const& error = LuaHelpers::get_last_error_context();
			REQUIRE(error.result == LUA_ERRRUN);
			REQUIRE(error.message == "attempt to call a table value");
			REQUIRE(error.source_name.empty());
			REQUIRE(error.line == 0);
		}

		SECTION("Lua-function that throws an error")
		{
			std::string_view const script = R"str(function bar(a)
	return a + 2
end
function foo(a)
	local c = bar('string')
	return c
end
)str";

			REQUIRE(LuaHelpers::load_script_inline(L, "inline_error_chunk", script));
			REQUIRE(lua_gettop(L) == 2);

			lua_getfenv(L, -1);
			lua_insert(L, -2);
			REQUIRE(LuaHelpers::pcall(L, 0, 0, false));

			lua_getfield(L, -1, "foo");
			REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);
			lua_replace(L, -2);

			REQUIRE(!LuaHelpers::pcall(L, 0, 1, false));

			LuaHelpers::ErrorContext const& error = LuaHelpers::get_last_error_context();
			REQUIRE(error.result == LUA_ERRRUN);
			REQUIRE(error.message.find("inline_error_chunk") != std::string::npos);
			REQUIRE(error.source_name == "[string \"inline_error_chunk\"]");
			REQUIRE(error.line == 2);
		}

		REQUIRE(lua_gettop(L) == 1);
		REQUIRE(lua_iscfunction(L, 1));
	}

	SECTION("debug_dump_stack")
	{
		lua_pushnil(L);
		lua_pushboolean(L, true);
		lua_pushlightuserdata(L, L);
		lua_pushnumber(L, 1234.5);
		lua_pushstring(L, "Spoons! Forks!");
		lua_createtable(L, 0, 0);
		lua_pushcfunction(L, &at_panic);
		lua_newuserdata(L, 4);
		lua_pushthread(L);

		REQUIRE(lua_gettop(L) == 9);
		std::stringstream stream;
		LuaHelpers::debug_dump_stack(stream, L, "unittest");
		REQUIRE(lua_gettop(L) == 9);

		std::string dump                    = std::move(stream).str();
		std::vector<std::string_view> lines = split_string(dump);

		REQUIRE(lines.size() == 10);
		REQUIRE(lines[0].starts_with("=="));
		REQUIRE(lines[0].find("unittest") != std::string_view::npos);
		REQUIRE(lines[1] == "1: nil");
		REQUIRE(lines[2] == "2: true");
		REQUIRE(lines[3].starts_with("3: userdata: "));
		REQUIRE(lines[4] == "4: 1234.5");
		REQUIRE(lines[5] == "5: Spoons! Forks!");
		REQUIRE(lines[6].starts_with("6: table: "));
		REQUIRE(lines[7].starts_with("7: function: "));
		REQUIRE(lines[8].starts_with("8: userdata: "));
		REQUIRE(lines[9].starts_with("9: thread: "));
	}

	lua_close(L);
}
