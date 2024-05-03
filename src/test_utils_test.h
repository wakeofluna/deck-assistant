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
bool get_and_pop_key_value_in_table(lua_State* L, int idx);

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
