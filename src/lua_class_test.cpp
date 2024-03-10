#include "lua_class.h"
#include "lua_helpers.h"
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

} // namespace lua_class_test

using namespace lua_class_test;

#include "lua_class.hpp"
template class LuaClass<TestClassVariant1>;
template class LuaClass<TestClassVariant2>;
template class LuaClass<TestClassVariant3>;
template class LuaClass<TestClassVariant4>;

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
			TestClassVariant1::create_new(L);
			static_assert(!has_finalize<TestClassVariant1>(is_available()));

			REQUIRE(g_clz1_constructed == 1);

			lua_getmetatable(L, -1);
			lua_getfield(L, -1, "__gc");
			REQUIRE(lua_type(L, -1) == LUA_TNIL);
		}

		SECTION("Trivial destructor with finalizer")
		{
			lua_pushliteral(L, "canary");

			TestClassVariant2::create_new(L);
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
			TestClassVariant3::create_new(L);
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
			TestClassVariant4::create_new(L);
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

		SECTION("Cleanup of instance table and userdata table")
		{
			LuaHelpers::push_userdata_container(L);
			LuaHelpers::push_instance_table_container(L);
			TestClassVariant1* tc1 = TestClassVariant1::create_new(L);

			TableCountMap count_map1_before = count_elements_in_table(L, 1);
			REQUIRE(count_map1_before[LUA_TLIGHTUSERDATA].first == 1);
			REQUIRE(count_map1_before[LUA_TUSERDATA].second == 1);
			TableCountMap count_map2_before = count_elements_in_table(L, 2);
			REQUIRE(count_map2_before[LUA_TUSERDATA].first == 1);
			REQUIRE(count_map2_before[LUA_TTABLE].second == 1);

			lua_pushlightuserdata(L, tc1);
			lua_gettable(L, 1);
			REQUIRE(lua_touserdata(L, -1) == tc1);
			lua_gettable(L, 2);
			REQUIRE(lua_type(L, -1) == LUA_TTABLE);

			lua_settop(L, 2);
			lua_gc(L, LUA_GCCOLLECT, 0);

			TableCountMap count_map1_after = count_elements_in_table(L, 1);
			REQUIRE(count_map1_after[LUA_TLIGHTUSERDATA].first == 0);
			REQUIRE(count_map1_after[LUA_TUSERDATA].second == 0);
			TableCountMap count_map2_after = count_elements_in_table(L, 2);
			REQUIRE(count_map2_after[LUA_TUSERDATA].first == 0);
			REQUIRE(count_map2_after[LUA_TTABLE].second == 0);
		}
	}

	SECTION("index and newindex")
	{
		SECTION("indexing into a class and instance table")
		{
			TestClassVariant1::create_new(L);
			TestClassVariant1::create_new(L);

			lua_getfield(L, 1, "table");
			REQUIRE(to_string_view(L, -1) == "TestClassVariant1:instance");

			lua_pushnil(L);
			lua_setfield(L, 1, "table");

			lua_getfield(L, 1, "table");
			REQUIRE(to_string_view(L, -1) == "TestClassVariant1:class");
			lua_getfield(L, 2, "table");
			REQUIRE(to_string_view(L, -1) == "TestClassVariant1:instance");

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
			TestClassVariant2::create_new(L);

			lua_getfield(L, 1, "table");
			REQUIRE(to_string_view(L, -1) == "TestClassVariant2:instance");

			lua_pushnil(L);
			lua_setfield(L, 1, "table");

			lua_getfield(L, 1, "table");
			REQUIRE(lua_isnil(L, -1));
		}

		SECTION("indexing without class or instance table")
		{
			TestClassVariant3::create_new(L);

			REQUIRE_THROWS(lua_getfield(L, 1, "table"));
			REQUIRE(to_string_view(L, -1) == "attempt to index a userdata value");
		}

		SECTION("indexing into a class table")
		{
			TestClassVariant4::create_new(L);

			lua_getfield(L, 1, "table");
			REQUIRE(to_string_view(L, -1) == "TestClassVariant4:class");
			lua_getfield(L, 1, "value");
			REQUIRE(lua_tointeger(L, -1) == 44);

			lua_pushnil(L);
			REQUIRE_THROWS(lua_setfield(L, 1, "table"));
			REQUIRE(to_string_view(L, -1) == "attempt to index a userdata value");

			lua_getfield(L, 1, "table");
			REQUIRE(to_string_view(L, -1) == "TestClassVariant4:class");
		}
	}

	SECTION("create_new")
	{
		TestClassVariant1* tc1 = TestClassVariant1::create_new(L);
		REQUIRE(lua_gettop(L) == 1);
		REQUIRE(lua_touserdata(L, -1) == tc1);

		lua_getmetatable(L, -1);
		luaL_getmetatable(L, tc1->type_name());
		REQUIRE(lua_rawequal(L, -2, -1));
		lua_pop(L, 1);

		lua_getfield(L, -1, "__name");
		REQUIRE(to_string_view(L, -1) == tc1->type_name());
	}

	lua_close(L);
}
