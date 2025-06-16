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

#ifndef DECK_ASSISTANT_LUA_CLASS_HPP_
#define DECK_ASSISTANT_LUA_CLASS_HPP_

#include "lua_class.h"
#include "lua_helpers.h"
#include <cassert>
#include <lua.hpp>
#include <string_view>
#include <type_traits>
#include <typeinfo>

// Needed to safely use std::string_view in functions that can error (longjmp)
static_assert(std::is_trivially_destructible_v<std::string_view>);

namespace
{
#define TPTR  ((T*)0)
#define TCPTR ((const T*)0)
#define TREF  (*(const T*)0)
#define LPTR  ((lua_State*)0)

struct not_available
{
};
struct is_available : public not_available
{
};

// *************************** TYPENAME *************************

template <typename T>
constexpr inline char const* __typename(not_available)
{
	return "(no typename available)"; // typeid(T).name();
}

template <typename T>
constexpr inline char const* __typename(std::enable_if_t<
                                        std::is_same_v<char const*, decltype(T::LUA_TYPENAME)>,
                                        is_available>)
{
	return T::LUA_TYPENAME;
}

template <typename T>
constexpr inline char const* __typename()
{
	return __typename<T>(is_available());
}

// *************************** GLOBAL INDEX *************************

template <typename T>
constexpr inline bool is_global(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool is_global(std::enable_if_t<
                                std::is_same_v<bool const, decltype(T::LUA_IS_GLOBAL)>,
                                is_available>)
{
	return T::LUA_IS_GLOBAL;
}

template <typename T>
constexpr inline bool is_global()
{
	return is_global<T>(is_available());
}

template <typename T>
constexpr inline bool is_push_this_enabled(not_available)
{
	return is_global<T>(is_available());
}

template <typename T>
constexpr inline bool is_push_this_enabled(std::enable_if_t<
                                           std::is_same_v<bool const, decltype(T::LUA_ENABLE_PUSH_THIS)>,
                                           is_available>)
{
	return is_global<T>(is_available()) || T::LUA_ENABLE_PUSH_THIS;
}

template <typename T>
constexpr inline bool is_push_this_enabled()
{
	return is_push_this_enabled<T>(is_available());
}

// *************************** GC FINALIZE *************************

template <typename T>
constexpr inline bool has_finalize(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_finalize(std::enable_if_t<std::is_same_v<void, decltype(TPTR->finalize(LPTR))>,
                                                    is_available>)
{
	return true;
}

template <typename T>
constexpr inline bool has_finalize()
{
	return has_finalize<T>(is_available());
}

template <typename T>
int __gc(lua_State* L)
{
	T* object = reinterpret_cast<T*>(lua_touserdata(L, 1));
	if constexpr (has_finalize<T>())
		object->finalize(L);

	if constexpr (is_push_this_enabled<T>())
	{
		lua_getmetatable(L, 1);
		lua_rawgeti(L, -1, LuaHelpers::IDX_META_INSTANCELIST);
		luaL_unref(L, -1, object->get_lua_ref_id());
		lua_pop(L, 2);
	}

	object->~T();
	return 0;
}

template <typename T>
void register_gc(lua_State* L)
{
	if constexpr (has_finalize<T>() || !std::is_trivially_destructible_v<T> || is_push_this_enabled<T>())
	{
		lua_pushcfunction(L, &__gc<T>);
		lua_setfield(L, -2, "__gc");
	}
}

// *************************** INIT CLASS *************************

template <typename T>
constexpr inline bool has_init_class_table(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_init_class_table(std::enable_if_t<
                                           std::is_same_v<void, decltype(T::init_class_table(LPTR))>,
                                           is_available>)
{
	return true;
}

template <typename T>
inline bool __init_class_table(lua_State* L)
{
	int oldtop = lua_gettop(L);
	T::init_class_table(L);
	(void)oldtop;
	assert(lua_gettop(L) == oldtop);
	return true;
}

// *************************** INIT INSTANCE *************************

template <typename T>
constexpr inline bool has_init_instance_table(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_init_instance_table(std::enable_if_t<
                                              std::is_same_v<void, decltype(TPTR->init_instance_table(LPTR))>,
                                              is_available>)
{
	return true;
}

template <typename T>
inline bool __init_instance_table(lua_State* L, T* obj)
{
	int oldtop = lua_gettop(L);
	obj->init_instance_table(L);
	(void)oldtop;
	assert(lua_gettop(L) == oldtop);
	return true;
}

// *************************** TOSTRING *************************

template <typename T>
int __tostring1(lua_State* L)
{
	T const* object = reinterpret_cast<T*>(lua_touserdata(L, -1));
	lua_pushstring(L, object->tostring());
	return 1;
}

template <typename T>
int __tostring2(lua_State* L)
{
	T const* object = reinterpret_cast<T*>(lua_touserdata(L, -1));
	return object->tostring(L);
}

template <typename T>
int __tostring3(lua_State* L)
{
	T const* object = reinterpret_cast<T*>(lua_touserdata(L, -1));
	lua_pushfstring(L, "%s: %p", __typename<T>(), object);
	return 1;
}

template <typename T>
constexpr inline bool register_tostring(lua_State* L, not_available)
{
	lua_pushcfunction(L, &__tostring3<T>);
	lua_setfield(L, -2, "__tostring");
	return true;
}

template <typename T>
inline bool register_tostring(lua_State* L, std::enable_if_t<
                                                std::is_same_v<char const*, decltype(TCPTR->tostring())>,
                                                is_available>)
{
	lua_pushcfunction(L, &__tostring1<T>);
	lua_setfield(L, -2, "__tostring");
	return true;
}

template <typename T>
inline bool register_tostring(lua_State* L, std::enable_if_t<
                                                std::is_same_v<int, decltype(TCPTR->tostring(LPTR))>,
                                                is_available>)
{
	lua_pushcfunction(L, &__tostring2<T>);
	lua_setfield(L, -2, "__tostring");
	return true;
}

// *************************** EQ *************************

template <typename T>
int __eq(lua_State* L)
{
	T const* object1 = LuaClass<T>::from_stack(L, -2, true);
	T const* object2 = LuaClass<T>::from_stack(L, -1, true);
	lua_pushboolean(L, object1->operator==(*object2));
	return 1;
}

template <typename>
constexpr inline bool register_eq(lua_State* L, not_available)
{
	return false;
}

template <typename T>
inline bool register_eq(lua_State* L, std::enable_if_t<
                                          std::is_same_v<bool, decltype(TCPTR->operator==(TREF))>,
                                          is_available>)
{
	lua_pushcfunction(L, &__eq<T>);
	lua_setfield(L, -2, "__eq");
	return true;
}

// *************************** LT *************************

template <typename T>
int __lt(lua_State* L)
{
	T const* object1 = LuaClass<T>::from_stack(L, -2, true);
	T const* object2 = LuaClass<T>::from_stack(L, -1, true);
	lua_pushboolean(L, object1->operator<(*object2));
	return 1;
}

template <typename>
constexpr inline bool register_lt(lua_State* L, not_available)
{
	return false;
}

template <typename T>
inline bool register_lt(lua_State* L, std::enable_if_t<
                                          std::is_same_v<bool, decltype(TCPTR->operator<(TREF))>,
                                          is_available>)
{
	lua_pushcfunction(L, &__lt<T>);
	lua_setfield(L, -2, "__lt");
	return true;
}

// *************************** LE *************************

template <typename T>
int __le(lua_State* L)
{
	T const* object1 = LuaClass<T>::from_stack(L, -2, true);
	T const* object2 = LuaClass<T>::from_stack(L, -1, true);
	lua_pushboolean(L, object1->operator<=(*object2));
	return 1;
}

template <typename>
constexpr inline bool register_le(lua_State* L, not_available)
{
	return false;
}

template <typename T>
inline bool register_le(lua_State* L, std::enable_if_t<
                                          std::is_same_v<bool, decltype(TCPTR->operator<=(TREF))>,
                                          is_available>)
{
	lua_pushcfunction(L, &__le<T>);
	lua_setfield(L, -2, "__le");
	return true;
}

// *************************** INDEX *************************

template <typename T>
constexpr inline bool has_index_int(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_index_int(std::enable_if_t<
                                    std::is_same_v<int, decltype(TPTR->index(LPTR, lua_Integer(2)))>,
                                    is_available>)
{
	return true;
}

template <typename T>
constexpr inline bool has_index_string(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_index_string(std::enable_if_t<
                                       std::is_same_v<int, decltype(TPTR->index(LPTR, std::string_view()))>,
                                       is_available>)
{
	return true;
}

template <typename T>
constexpr inline bool has_index_full(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_index_full(std::enable_if_t<
                                     std::is_same_v<int, decltype(TPTR->index(LPTR))>,
                                     is_available>)
{
	return true;
}

template <typename T>
constexpr inline bool has_index(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_index(std::enable_if_t<
                                has_index_full<T>(is_available()) || has_index_int<T>(is_available()) || has_index_string<T>(is_available()),
                                is_available>)
{
	return true;
}

template <typename T>
int __index(lua_State* L)
{
	constexpr bool const hasClassTable  = has_init_class_table<T>(is_available());
	constexpr bool const hasIndexInt    = has_index_int<T>(is_available());
	constexpr bool const hasIndexString = has_index_string<T>(is_available());
	constexpr bool const hasIndexFull   = has_index_full<T>(is_available());
	constexpr bool const hasIndex       = hasIndexInt || hasIndexString || hasIndexFull;

	LuaHelpers::push_instance_table(L, 1);
	lua_pushvalue(L, 2);
	lua_rawget(L, -2);
	if (lua_type(L, -1) != LUA_TNIL)
		return 1;
	lua_pop(L, 2);

	if constexpr (hasClassTable)
	{
		LuaHelpers::push_class_table(L, 1);
		lua_pushvalue(L, 2);
		lua_rawget(L, -2);
		if (lua_type(L, -1) != LUA_TNIL)
			return 1;
		lua_pop(L, 2);
	}

	if constexpr (hasIndex)
	{
		T* object = reinterpret_cast<T*>(lua_touserdata(L, -2));

		if constexpr (hasIndexInt || hasIndexString)
		{
			int index_type = lua_type(L, -1);

			if constexpr (hasIndexInt)
			{
				if (index_type == LUA_TNUMBER)
				{
					lua_Integer number = lua_tointeger(L, -1);
					return object->index(L, number);
				}
			}

			if constexpr (hasIndexString)
			{
				if (index_type == LUA_TSTRING)
				{
					size_t len;
					char const* str = lua_tolstring(L, -1, &len);
					return object->index(L, std::string_view(str, len));
				}
			}
		}

		if constexpr (hasIndexFull)
		{
			return object->index(L);
		}
	}

	lua_pushnil(L);
	return 1;
}

template <typename T>
inline bool register_index(lua_State* L)
{
	lua_pushcfunction(L, &__index<T>);
	lua_setfield(L, -2, "__index");
	return true;
}

// *************************** NEWINDEX *************************

template <typename T>
constexpr inline bool has_newindex_int(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_newindex_int(std::enable_if_t<
                                       std::is_same_v<int, decltype(TPTR->newindex(LPTR, lua_Integer(2)))>,
                                       is_available>)
{
	return true;
}

template <typename T>
constexpr inline bool has_newindex_string(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_newindex_string(std::enable_if_t<
                                          std::is_same_v<int, decltype(TPTR->newindex(LPTR, std::string_view()))>,
                                          is_available>)
{
	return true;
}

template <typename T>
constexpr inline bool has_newindex_full(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_newindex_full(std::enable_if_t<
                                        std::is_same_v<int, decltype(TPTR->newindex(LPTR))>,
                                        is_available>)
{
	return true;
}

template <typename T>
constexpr inline bool has_newindex(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_newindex(std::enable_if_t<
                                   has_newindex_full<T>(is_available()) || has_newindex_int<T>(is_available()) || has_newindex_string<T>(is_available()),
                                   is_available>)
{
	return true;
}

template <typename T>
int __newindex(lua_State* L)
{
	constexpr bool const hasClassTable  = has_init_class_table<T>(is_available());
	constexpr bool const hasIndexInt    = has_newindex_int<T>(is_available());
	constexpr bool const hasIndexString = has_newindex_string<T>(is_available());
	constexpr bool const hasIndexFull   = has_newindex_full<T>(is_available());
	constexpr bool const hasIndex       = hasIndexInt || hasIndexString || hasIndexFull;

	if constexpr (hasClassTable)
	{
		// If the value exists in the class table, this new value must be of the same type or nil

		LuaHelpers::push_class_table(L, 1);
		lua_pushvalue(L, 2);
		lua_rawget(L, -2);

		int const class_type = lua_type(L, -1);
		int const value_type = lua_type(L, 3);
		if (value_type != LUA_TNIL && class_type != LUA_TNIL && value_type != class_type)
			luaL_typerror(L, 3, lua_typename(L, class_type));

		lua_pop(L, 2);
	}

	if constexpr (hasIndex)
	{
		T* object = reinterpret_cast<T*>(lua_touserdata(L, -3));

		if constexpr (hasIndexInt || hasIndexString)
		{
			int key_type = lua_type(L, -2);

			if constexpr (hasIndexInt)
			{
				if (key_type == LUA_TNUMBER)
				{
					lua_Integer number = lua_tointeger(L, -2);
					return object->newindex(L, number);
				}
			}

			if constexpr (hasIndexString)
			{
				if (key_type == LUA_TSTRING)
				{
					size_t len;
					char const* str = lua_tolstring(L, -2, &len);
					return object->newindex(L, std::string_view(str, len));
				}
			}
		}

		if constexpr (hasIndexFull)
		{
			return object->newindex(L);
		}
	}

	LuaHelpers::push_instance_table(L, -3);
	lua_replace(L, -4);
	lua_rawset(L, -3);
	return 0;
}

template <typename T>
inline bool register_newindex(lua_State* L)
{
	lua_pushcfunction(L, &__newindex<T>);
	lua_setfield(L, -2, "__newindex");
	return true;
}

// *************************** CALL *************************

template <typename T>
int __call(lua_State* L)
{
	T* object = reinterpret_cast<T*>(lua_touserdata(L, 1));
	return object->call(L);
}

template <typename T>
constexpr inline bool register_call(lua_State* L, not_available)
{
	return false;
}

template <typename T>
inline bool register_call(lua_State* L, std::enable_if_t<
                                            std::is_same_v<int, decltype(TPTR->call(LPTR))>,
                                            is_available>)
{
	lua_pushcfunction(L, &__call<T>);
	lua_setfield(L, -2, "__call");
	return true;
}
} // namespace

template <typename T>
T* LuaClass<T>::check_global_instance(lua_State* L)
{
	if constexpr (is_global<T>())
	{
		T* instance = push_global_instance(L);
		if (instance)
			return instance;

		lua_pop(L, 1);
	}
	return nullptr;
}

template <typename T>
void LuaClass<T>::finish_initialisation(lua_State* L, T* object)
{
	push_metatable(L);

	if constexpr (is_push_this_enabled<T>())
	{
		lua_rawgeti(L, -1, LuaHelpers::IDX_META_INSTANCELIST);
		lua_pushvalue(L, -3);
		object->m_lua_ref_id = luaL_ref(L, -2);
		lua_pop(L, 1);
	}

	if constexpr (is_global<T>())
	{
		lua_pushvalue(L, -2);
		lua_rawseti(L, -2, LuaHelpers::IDX_META_GLOBAL_INSTANCE);
	}

	lua_setmetatable(L, -2);

	lua_newtable(L);
	if constexpr (has_init_instance_table<T>(is_available()))
		__init_instance_table<T>(L, object);
	lua_setfenv(L, -2);
}

template <typename T>
void LuaClass<T>::push_instance_list_table(lua_State* L)
{
	push_metatable(L);
	lua_rawgeti(L, -1, LuaHelpers::IDX_META_INSTANCELIST);
	lua_replace(L, -2);
}

template <typename T>
void LuaClass<T>::push_this(lua_State* L)
{
	if constexpr (is_push_this_enabled<T>())
	{
		push_instance_list_table(L);
		lua_rawgeti(L, -1, m_lua_ref_id);
		lua_replace(L, -2);
	}
	else
	{
		assert(false && "attempted to push_this of non-push-this-enabled class");
	}
}

template <typename T>
void LuaClass<T>::set_lua_ref_id(int ref_id)
{
	if constexpr (is_push_this_enabled<T>())
	{
		assert(false && "attempted to set lua_ref_id on push-this-enabled class");
	}
	else
	{
		m_lua_ref_id = ref_id;
	}
}

template <typename T>
T* LuaClass<T>::push_global_instance(lua_State* L)
{
	if constexpr (is_global<T>())
	{
		push_metatable(L);
		lua_rawgeti(L, -1, LuaHelpers::IDX_META_GLOBAL_INSTANCE);
		lua_replace(L, -2);
		return from_stack(L, -1, false);
	}
	else
	{
		assert(false && "attempted to get global instance of non-global class");
		return nullptr;
	}
}

template <typename T>
T* LuaClass<T>::push_from_ref_id(lua_State* L, int lua_ref_id)
{
	if constexpr (is_push_this_enabled<T>())
	{
		push_instance_list_table(L);
		lua_rawgeti(L, -1, lua_ref_id);

		T* instance = from_stack(L, -1, false);
		assert(instance || lua_isnil(L, -1));

		if (instance)
			lua_replace(L, -2);
		else
			lua_pop(L, 2);

		return instance;
	}
	else
	{
		assert(false && "attempted to push_from_ref_id of non-push-this-enabled class");
		return nullptr;
	}
}

template <typename T>
T* LuaClass<T>::from_stack(lua_State* L, int idx, bool throw_error)
{
	bool ok = false;

	if (lua_type(L, idx) == LUA_TUSERDATA && lua_getmetatable(L, idx))
	{
		ok = (lua_type(L, -1) == LUA_TTABLE && lua_topointer(L, -1) == T::m_metatable_ptr);
		lua_pop(L, 1);
	}

	if (ok)
		return reinterpret_cast<T*>(lua_touserdata(L, idx));

	if (throw_error)
	{
		idx = LuaHelpers::absidx(L, idx);
		luaL_typerror(L, idx, __typename<T>());
	}

	return nullptr;
}

template <typename T>
char const* LuaClass<T>::type_name()
{
	return __typename<T>();
}

template <typename T>
int LuaClass<T>::push_metatable(lua_State* L)
{
	char const* tname = __typename<T>();
	lua_getfield(L, LUA_REGISTRYINDEX, tname);
	if (lua_type(L, -1) != LUA_TTABLE)
	{
		lua_pop(L, 1);
		lua_createtable(L, 2, 12);

		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, tname);

		lua_pushstring(L, tname);
		lua_pushvalue(L, -1);
		lua_setfield(L, -3, "__name");
		lua_setfield(L, -2, "__metatable");

		if constexpr (has_init_class_table<T>(is_available()))
		{
			// Class table
			lua_createtable(L, 0, 8);
			__init_class_table<T>(L);
			lua_rawseti(L, -2, LuaHelpers::IDX_META_CLASSTABLE);
		}

		if constexpr (is_push_this_enabled<T>())
		{
			// Instance list table
			if constexpr (is_global<T>())
				lua_createtable(L, 1, 0);
			else
				lua_createtable(L, 32, 0);

			LuaHelpers::push_standard_weak_value_metatable(L);
			lua_setmetatable(L, -2);

			lua_rawseti(L, -2, LuaHelpers::IDX_META_INSTANCELIST);
		}

		register_gc<T>(L);
		register_tostring<T>(L, is_available());
		register_eq<T>(L, is_available());
		register_lt<T>(L, is_available());
		register_le<T>(L, is_available());
		register_index<T>(L);
		register_newindex<T>(L);
		register_call<T>(L, is_available());

		T::m_metatable_ptr = lua_topointer(L, -1);
	}
	return 1;
}

#endif // DECK_ASSISTANT_LUA_CLASS_HPP_
