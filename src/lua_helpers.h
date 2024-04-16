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

#ifndef DECK_ASSISTANT_LUA_HELPERS_H
#define DECK_ASSISTANT_LUA_HELPERS_H

#include <filesystem>
#include <iosfwd>
#include <lua.hpp>
#include <string>
#include <string_view>

// Compatibility between luajit and lua5.1
#ifndef LUA_OK
#define LUA_OK 0
#endif

namespace LuaHelpers
{

// Indices in class metatable
constexpr lua_Integer const IDX_META_CLASSTABLE      = 1;
constexpr lua_Integer const IDX_META_INSTANCELIST    = 2;
constexpr lua_Integer const IDX_META_GLOBAL_INSTANCE = 3;

struct ErrorContext
{
	void clear();

	int result;
	std::string message;
	std::string source_name;
	int line;
};

enum class Trust : char
{
	// Cannot do anything scary: call debug commands, access the filesystem outside the sandbox, make network connections outside localhost
	Untrusted = 1,

	// Can load scripts (max level: trusted), can make network connections
	Trusted = 2,

	// Can do everything
	Admin = 3,
};

int absidx(lua_State* L, int idx);

void push_standard_weak_key_metatable(lua_State* L);
void push_standard_weak_value_metatable(lua_State* L);
void push_yielded_calls_table(lua_State* L);
void push_global_environment_table(lua_State* L, Trust trust);

void push_class_table(lua_State* L, int idx);
void push_instance_table(lua_State* L, int idx);
void push_instance_list_table(lua_State* L, int idx);

std::string_view to_string_view(lua_State* L, int idx);
std::string_view check_arg_string(lua_State* L, int idx, bool allow_empty = false);
std::string_view check_arg_string_or_none(lua_State* L, int idx);
lua_Integer check_arg_int(lua_State* L, int idx);
bool check_arg_bool(lua_State* L, int idx);
std::string_view push_converted_to_string(lua_State* L, int idx);

void newindex_store_in_instance_table(lua_State* L);
void copy_table_fields(lua_State* L);

template <typename... ARGS>
bool emit_event(lua_State* L, int idx, char const* function_name, ARGS... args);

bool load_script(lua_State* L, std::filesystem::path const& file, Trust trust, bool log_error = true);
bool load_script_inline(lua_State* L, char const* chunk_name, std::string_view const& script, Trust trust, bool log_error = true);
void assign_new_env_table(lua_State* L, int idx, char const* chunk_name, Trust trust);

bool pcall(lua_State* L, int nargs, int nresults, bool log_error = true);
bool yieldable_call(lua_State* L, int nargs, bool log_error = true);
bool lua_lineinfo(lua_State* L, std::string& short_src, int& currentline);
ErrorContext const& get_last_error_context();

void debug_dump_stack(std::ostream& stream, lua_State* L, char const* description = nullptr);
void debug_dump_stack(lua_State* L, char const* description = nullptr);
void debug_dump_table(std::ostream& stream, lua_State* L, int idx, bool recursive = false, char const* description = nullptr);
void debug_dump_table(lua_State* L, int idx, bool recursive = false, char const* description = nullptr);

struct StackValue
{
	StackValue(int idx)
	    : index(idx)
	{
	}

	StackValue(lua_State* L, int idx)
	    : index(absidx(L, idx))
	{
	}

	StackValue(StackValue const&) = default;
	StackValue(StackValue&&)      = default;

	int index;
};

inline void push_argument(lua_State* L, bool value)
{
	lua_pushboolean(L, value);
}

inline void push_argument(lua_State* L, int value)
{
	lua_pushinteger(L, value);
}

inline void push_argument(lua_State* L, long value)
{
	lua_pushinteger(L, value);
}

inline void push_argument(lua_State* L, long long value)
{
	lua_pushinteger(L, value);
}

inline void push_argument(lua_State* L, unsigned int value)
{
	lua_pushinteger(L, value);
}

inline void push_argument(lua_State* L, unsigned long value)
{
	lua_pushinteger(L, value);
}

inline void push_argument(lua_State* L, unsigned long long value)
{
	lua_pushinteger(L, value);
}

inline void push_argument(lua_State* L, char const* value)
{
	lua_pushstring(L, value);
}

inline void push_argument(lua_State* L, std::string_view const& value)
{
	lua_pushlstring(L, value.data(), value.size());
}

inline void push_argument(lua_State* L, std::string const& value)
{
	lua_pushlstring(L, value.data(), value.size());
}

inline void push_argument(lua_State* L, StackValue value)
{
	lua_pushvalue(L, value.index);
}

template <typename T, typename... ARGS>
inline void push_arguments(lua_State* L, T arg, ARGS... args)
{
	push_argument(L, std::forward<T>(arg));
	if constexpr (sizeof...(args) > 0)
		push_arguments(L, std::forward<ARGS>(args)...);
}

} // namespace LuaHelpers

template <typename... ARGS>
bool LuaHelpers::emit_event(lua_State* L, int idx, char const* function_name, ARGS... args)
{
	idx = absidx(L, idx);
	lua_getfield(L, idx, function_name);
	if (lua_type(L, -1) == LUA_TFUNCTION)
	{
		bool const is_userdata = lua_isuserdata(L, idx);
		if (is_userdata)
			lua_pushvalue(L, idx);

		constexpr int const nargs = sizeof...(args);
		if constexpr (nargs > 0)
			push_arguments(L, std::forward<ARGS>(args)...);

		return LuaHelpers::yieldable_call(L, (is_userdata ? 1 : 0) + nargs);
	}
	else
	{
		lua_pop(L, 1);
		return false;
	}
}

#endif // DECK_ASSISTANT_LUA_HELPERS_H
