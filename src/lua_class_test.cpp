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
// #include "lua_helpers.h"
#include "test_utils_test.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

namespace lua_class_test
{

int g_clz1_constructed = 0;
int g_clz2_constructed = 0;
int g_clz2_destructed  = 0;
int g_clz3_constructed = 0;
int g_clz3_destructed  = 0;
int g_clz4_constructed = 0;
int g_clz4_destructed  = 0;

struct DestructorCounter
{
	int& counter;
	DestructorCounter(int& destructs)
	    : counter(destructs)
	{
	}
	~DestructorCounter() { ++counter; }
};

class TestClassVariant1 : public LuaClass<TestClassVariant1>
{
public:
	// Trivial destructor, should not have gc. Can't track destructs.
	TestClassVariant1() { ++g_clz1_constructed; }

	// Both class and instance table
	static void init_class_table(lua_State* L)
	{
		lua_pushliteral(L, "TestClassVariant1:class");
		lua_setfield(L, -2, "table");

		lua_pushinteger(L, 11);
		lua_setfield(L, -2, "value");
	}

	void init_instance_table(lua_State* L)
	{
		lua_pushliteral(L, "TestClassVariant1:instance");
		lua_setfield(L, -2, "table");
	}
};

class TestClassVariant2 : public LuaClass<TestClassVariant2>
{
public:
	// Trivial destructor but with finalizer function. Should have gc
	TestClassVariant2()
	{
		++g_clz2_constructed;
	}

	// Only an instance table
	void init_instance_table(lua_State* L)
	{
		lua_pushliteral(L, "TestClassVariant2:instance");
		lua_setfield(L, -2, "table");
	}

	void finalize(lua_State* L)
	{
		TestClassVariant2* tc = from_stack(L, 1);
		REQUIRE(this == tc);
		++g_clz2_destructed;
	}
};

class TestClassVariant3 : public LuaClass<TestClassVariant3>
{
public:
	// Non-trivial implicit destructor, should have gc
	DestructorCounter dc;
	TestClassVariant3()
	    : dc(g_clz3_destructed)
	{
		++g_clz3_constructed;
	}

	// Neither a class nor instance table
};

class TestClassVariant4 : public LuaClass<TestClassVariant4>
{
public:
	// Explicit destructor, should have gc
	TestClassVariant4() { ++g_clz4_constructed; }
	~TestClassVariant4() { ++g_clz4_destructed; }

	// Only a class table
	static void init_class_table(lua_State* L)
	{
		lua_pushliteral(L, "TestClassVariant4:class");
		lua_setfield(L, -2, "table");

		lua_pushinteger(L, 44);
		lua_setfield(L, -2, "value");
	}
};

class TestClassVariant5 : public LuaClass<TestClassVariant5>
{
public:
	static constexpr bool const LUA_ENABLE_PUSH_THIS = true;
};

class TestClassVariant6 : public LuaClass<TestClassVariant6>
{
public:
	static constexpr bool const LUA_IS_GLOBAL = true;
};

} // namespace lua_class_test

using namespace lua_class_test;

#include "lua_class.hpp"
template class LuaClass<TestClassVariant1>;
template class LuaClass<TestClassVariant2>;
template class LuaClass<TestClassVariant3>;
template class LuaClass<TestClassVariant4>;
template class LuaClass<TestClassVariant5>;
template class LuaClass<TestClassVariant6>;

TEST_CASE("LuaClass", "[lua]")
{
	lua_State* L = new_test_state();

	g_clz1_constructed = 0;
	g_clz2_constructed = 0;
	g_clz2_destructed  = 0;
	g_clz3_constructed = 0;
	g_clz3_destructed  = 0;
	g_clz4_constructed = 0;
	g_clz4_destructed  = 0;

	REQUIRE(lua_gettop(L) == 0);

	SECTION("gc")
	{
		SECTION("Trivial destructor no gc")
		{
			TestClassVariant1::push_new(L);
			static_assert(!has_finalize<TestClassVariant1>(is_available()));

			REQUIRE(g_clz1_constructed == 1);

			lua_getmetatable(L, -1);
			lua_getfield(L, -1, "__gc");
			REQUIRE(lua_type(L, -1) == LUA_TNIL);
		}

		SECTION("Trivial destructor with finalizer")
		{
			lua_pushliteral(L, "canary");

			TestClassVariant2::push_new(L);
			REQUIRE(g_clz2_constructed == 1);

			static_assert(has_finalize<TestClassVariant2>(is_available()));
			lua_getmetatable(L, -1);
			lua_getfield(L, -1, "__gc");
			REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

			lua_pop(L, 3);
			lua_gc(L, LUA_GCCOLLECT, 0);

			REQUIRE(g_clz2_destructed == 1);
			REQUIRE(g_clz2_constructed == 1);
		}

		SECTION("Non-trivial implicit destructor")
		{
			TestClassVariant3::push_new(L);
			REQUIRE(g_clz3_constructed == 1);

			static_assert(!has_finalize<TestClassVariant3>(is_available()));
			lua_getmetatable(L, -1);
			lua_getfield(L, -1, "__gc");
			REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

			lua_pop(L, 3);
			lua_gc(L, LUA_GCCOLLECT, 0);

			REQUIRE(g_clz3_destructed == 1);
			REQUIRE(g_clz3_constructed == 1);
		}

		SECTION("Explicit destructor")
		{
			TestClassVariant4::push_new(L);
			REQUIRE(g_clz4_constructed == 1);

			static_assert(!has_finalize<TestClassVariant4>(is_available()));
			lua_getmetatable(L, -1);
			lua_getfield(L, -1, "__gc");
			REQUIRE(lua_type(L, -1) == LUA_TFUNCTION);

			lua_pop(L, 3);
			lua_gc(L, LUA_GCCOLLECT, 0);

			REQUIRE(g_clz4_destructed == 1);
			REQUIRE(g_clz4_constructed == 1);
		}
	}

	SECTION("index and newindex")
	{
		SECTION("indexing into a class and instance table")
		{
			TestClassVariant1::push_new(L);
			TestClassVariant1::push_new(L);

			lua_getfield(L, 1, "table");
			REQUIRE(LuaHelpers::to_string_view(L, -1) == "TestClassVariant1:instance");

			lua_pushnil(L);
			lua_setfield(L, 1, "table");

			lua_getfield(L, 1, "table");
			REQUIRE(LuaHelpers::to_string_view(L, -1) == "TestClassVariant1:class");
			lua_getfield(L, 2, "table");
			REQUIRE(LuaHelpers::to_string_view(L, -1) == "TestClassVariant1:instance");

			lua_getfield(L, 1, "value");
			REQUIRE(lua_tointeger(L, -1) == 11);
			lua_pushinteger(L, 1111);
			lua_setfield(L, 1, "value");

			lua_getfield(L, 1, "value");
			REQUIRE(lua_tointeger(L, -1) == 1111);
			lua_getfield(L, 2, "value");
			REQUIRE(lua_tointeger(L, -1) == 11);

			lua_pushnil(L);
			lua_setfield(L, 1, "value");
			lua_getfield(L, 1, "value");
			REQUIRE(lua_tointeger(L, -1) == 11);
		}

		SECTION("indexing into an instance table")
		{
			TestClassVariant2::push_new(L);

			lua_getfield(L, 1, "table");
			REQUIRE(LuaHelpers::to_string_view(L, -1) == "TestClassVariant2:instance");

			lua_pushnil(L);
			lua_setfield(L, 1, "table");

			lua_getfield(L, 1, "table");
			REQUIRE(lua_isnil(L, -1));
		}

		SECTION("indexing without class or instance table")
		{
			TestClassVariant3::push_new(L);

			lua_getfield(L, 1, "table");
			REQUIRE(lua_isnil(L, -1));
		}

		SECTION("indexing into a class table")
		{
			TestClassVariant4::push_new(L);

			lua_getfield(L, 1, "table");
			REQUIRE(LuaHelpers::to_string_view(L, -1) == "TestClassVariant4:class");
			lua_getfield(L, 1, "value");
			REQUIRE(lua_tointeger(L, -1) == 44);

			lua_pushnil(L);
			lua_setfield(L, 1, "table");

			lua_getfield(L, 1, "table");
			REQUIRE(LuaHelpers::to_string_view(L, -1) == "TestClassVariant4:class");
		}
	}

	SECTION("push_new")
	{
		TestClassVariant1* tc1 = TestClassVariant1::push_new(L);
		REQUIRE(lua_gettop(L) == 1);
		REQUIRE(lua_touserdata(L, -1) == tc1);

		lua_getmetatable(L, -1);
		luaL_getmetatable(L, tc1->type_name());
		REQUIRE(lua_rawequal(L, -2, -1));
		lua_pop(L, 1);

		lua_getfield(L, -1, "__name");
		REQUIRE(LuaHelpers::to_string_view(L, -1) == tc1->type_name());
	}

	SECTION("push_this")
	{
		for (int i = 0; i < 10; ++i)
		{
			int const top = lua_gettop(L);

			TestClassVariant5* tc5 = TestClassVariant5::push_new(L);
			REQUIRE(lua_gettop(L) == top + 1);
			tc5->push_this(L);
			REQUIRE(lua_gettop(L) == top + 2);
			REQUIRE(lua_touserdata(L, -2) == tc5);
			REQUIRE(lua_touserdata(L, -1) == tc5);
			REQUIRE(lua_rawequal(L, -2, -1));
			lua_pop(L, 1);

			TestClassVariant6* tc6 = TestClassVariant6::push_new(L);
			REQUIRE(lua_gettop(L) == top + 2);
			tc6->push_this(L);
			REQUIRE(lua_gettop(L) == top + 3);
			REQUIRE(lua_touserdata(L, -2) == tc6);
			REQUIRE(lua_touserdata(L, -1) == tc6);
			REQUIRE(lua_rawequal(L, -2, -1));
			lua_pop(L, 1);
		}
	}

	SECTION("push_global_instance")
	{
		TestClassVariant6* tc6 = TestClassVariant6::push_new(L);
		REQUIRE(lua_gettop(L) == 1);
		REQUIRE(lua_touserdata(L, -1) == tc6);
		TestClassVariant6::push_global_instance(L);
		REQUIRE(lua_gettop(L) == 2);
		REQUIRE(lua_touserdata(L, -1) == tc6);
		REQUIRE(lua_rawequal(L, -2, -1));

		TestClassVariant6* tc6_2 = TestClassVariant6::push_new(L);
		REQUIRE(tc6_2 == tc6);
		REQUIRE(lua_gettop(L) == 3);
		REQUIRE(lua_touserdata(L, -1) == tc6);
		REQUIRE(lua_rawequal(L, -2, -1));
		TestClassVariant6::push_global_instance(L);
		REQUIRE(lua_gettop(L) == 4);
		REQUIRE(lua_touserdata(L, -1) == tc6);
		REQUIRE(lua_rawequal(L, -2, -1));
	}

	SECTION("Cleanup of instancelist table after garbage collection")
	{
		static int const NUM_INSTANCES = 5;
		static int const DEL_INSTANCE  = 2;

		TestClassVariant5* tc5[NUM_INSTANCES];

		for (int i = 0; i < NUM_INSTANCES; ++i)
			tc5[i] = TestClassVariant5::push_new(L);

		for (int i = 0; i < NUM_INSTANCES; ++i)
			REQUIRE(tc5[i]->get_lua_ref_id() == i + 1);

		TestClassVariant5::push_instance_list_table(L);
		REQUIRE(lua_type(L, -1) == LUA_TTABLE);
		REQUIRE(lua_objlen(L, -1) == NUM_INSTANCES);

		lua_gc(L, LUA_GCCOLLECT, 0);

		for (int i = 0; i < NUM_INSTANCES; ++i)
		{
			lua_rawgeti(L, -1, i + 1);
			REQUIRE(lua_touserdata(L, -1) == tc5[i]);
			lua_pop(L, 1);
		}

		lua_remove(L, DEL_INSTANCE + 1);
		lua_gc(L, LUA_GCCOLLECT, 0);

		for (int i = 0; i < NUM_INSTANCES; ++i)
		{
			lua_rawgeti(L, -1, i + 1);
			if (i == DEL_INSTANCE)
				REQUIRE(lua_isnil(L, -1));
			else
				REQUIRE(lua_touserdata(L, -1) == tc5[i]);
			lua_pop(L, 1);
		}

		TestClassVariant5* tc5_last = TestClassVariant5::push_new(L);
		REQUIRE(tc5_last->get_lua_ref_id() == DEL_INSTANCE + 1);
		tc5[DEL_INSTANCE] = tc5_last;
		lua_insert(L, -2);

		for (int i = 0; i < NUM_INSTANCES; ++i)
		{
			lua_rawgeti(L, -1, i + 1);
			REQUIRE(lua_touserdata(L, -1) == tc5[i]);
			lua_pop(L, 1);
		}
	}

	lua_close(L);
}
