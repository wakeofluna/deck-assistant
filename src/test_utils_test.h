#ifndef DECK_ASSISTANT_TEST_UTILS_TEST_H
#define DECK_ASSISTANT_TEST_UTILS_TEST_H

#include <csetjmp>
#include <iostream>
#include <lua.hpp>
#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

extern std::string g_panic_msg;
extern std::jmp_buf g_panic_jmp;

#ifdef HAVE_LUAJIT
#define EXPECT_ERROR(x) \
	do \
	{ \
		const int oldtop = lua_gettop(L); \
		REQUIRE_THROWS(x); \
		size_t panic_len; \
		char const* panic_str = lua_tolstring(L, -1, &panic_len); \
		g_panic_msg.assign(panic_str, panic_len); \
		lua_settop(L, oldtop); \
	} while (0)
#else
#define EXPECT_ERROR(x) \
	do \
	{ \
		const int oldtop = lua_gettop(L); \
		if (!setjmp(g_panic_jmp)) \
		{ \
			x; \
			FAIL("Expected error did not happen"); \
		} \
		lua_settop(L, oldtop); \
	} while (0)
#endif

int at_panic(lua_State* L);
lua_State* new_test_state();

int push_dummy_value(lua_State* L, int tp);

using TableCountMap = std::map<int, std::pair<int, int>>;
TableCountMap count_elements_in_table(lua_State* L, int idx);

std::vector<std::string_view> split_string(std::string_view const& text, char split_char = '\n');

std::optional<int> to_int(lua_State* L, int idx);

template <typename T>
std::ostream& operator<<(std::ostream& stream, std::optional<T> const& value)
{
	if (!value.has_value())
		stream << "(no value)";
	else
		stream << value.value();
	return stream;
}

#endif // DECK_ASSISTANT_TEST_UTILS_TEST_H
