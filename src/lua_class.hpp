#ifndef DECK_ASSISTANT_LUA_CLASS_HPP_
#define DECK_ASSISTANT_LUA_CLASS_HPP_

#include "lua_class.h"
#include "lua_class_private.h"
#include <cassert>
#include <lua.hpp>
#include <new>
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
	return typeid(T).name();
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

// *************************** GC FINALIZE *************************

template <typename T>
constexpr inline bool has_finalize(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_finalize(std::enable_if_t<std::is_same_v<bool, decltype(TPTR->finalize(LPTR))>,
                                                    is_available>)
{
	return true;
}

template <typename T>
int __gc(lua_State* L)
{
	T* object = reinterpret_cast<T*>(lua_touserdata(L, 1));
	if constexpr (has_finalize<T>(is_available()))
	{
		if (object->finalize())
			object->~T();
	}
	else
	{
		object->~T();
	}
	return 0;
}

template <typename T>
void register_gc(lua_State* L)
{
	if constexpr (has_finalize<T>(is_available()) || !std::is_trivially_destructible_v<T>)
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

// *************************** LEN *************************

template <typename T>
int __len(lua_State* L)
{
	T const* object = reinterpret_cast<T*>(lua_touserdata(L, -1));
	lua_pushinteger(L, object->length());
	return 1;
}

template <typename>
constexpr inline bool register_len(lua_State* L, not_available)
{
	return false;
}

template <typename T>
inline bool register_len(lua_State* L, std::enable_if_t<
                                           std::is_same_v<int, decltype(TCPTR->length())>,
                                           is_available>)
{
	lua_pushcfunction(L, &__len<T>);
	lua_setfield(L, -2, "__len");
	return true;
}

// *************************** TOSTRING *************************

template <typename T>
int __tostring1(lua_State* L)
{
	T const* object = reinterpret_cast<T*>(lua_touserdata(L, -1));
	lua_pushstring(L, object->to_string());
	return 1;
}

template <typename T>
int __tostring2(lua_State* L)
{
	T const* object = reinterpret_cast<T*>(lua_touserdata(L, -1));
	return object->to_string(L);
}

template <typename>
constexpr inline bool register_tostring(lua_State* L, not_available)
{
	return false;
}

template <typename T>
inline bool register_tostring(lua_State* L, std::enable_if_t<
                                                std::is_same_v<char const*, decltype(TCPTR->to_string())>,
                                                is_available>)
{
	lua_pushcfunction(L, &__tostring1<T>);
	lua_setfield(L, -2, "__tostring");
	return true;
}

template <typename T>
inline bool register_tostring(lua_State* L, std::enable_if_t<
                                                std::is_same_v<int, decltype(TCPTR->to_string(LPTR))>,
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
	constexpr bool const hasClassTable    = has_init_class_table<T>(is_available());
	constexpr bool const hasInstanceTable = has_init_instance_table<T>(is_available());
	constexpr bool const hasIndexInt      = has_index_int<T>(is_available());
	constexpr bool const hasIndexString   = has_index_string<T>(is_available());
	constexpr bool const hasIndexFull     = has_index_full<T>(is_available());
	constexpr bool const hasIndex         = hasIndexInt || hasIndexString || hasIndexFull;

	if constexpr (hasInstanceTable)
	{
		LuaHelpers::push_instance_table(L, 1);
		lua_pushvalue(L, 2);
		lua_rawget(L, -2);
		if (lua_type(L, -1) != LUA_TNIL)
			return 1;
		lua_pop(L, 2);
	}

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
					int isnum;
					lua_Integer number = lua_tointegerx(L, -1, &isnum);
					if (isnum)
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
inline bool register_index(lua_State* L, not_available)
{
	constexpr bool hasClassTable    = has_init_class_table<T>(is_available());
	constexpr bool hasInstanceTable = has_init_instance_table<T>(is_available());
	constexpr bool hasIndex         = has_index<T>(is_available());

	if constexpr (hasClassTable || hasInstanceTable || hasIndex)
	{
		lua_pushcfunction(L, &__index<T>);
		lua_setfield(L, -2, "__index");
		return true;
	}

	return false;
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
	constexpr bool const hasInstanceTable = has_init_instance_table<T>(is_available());
	constexpr bool const hasIndexInt      = has_newindex_int<T>(is_available());
	constexpr bool const hasIndexString   = has_newindex_string<T>(is_available());
	constexpr bool const hasIndexFull     = has_newindex_full<T>(is_available());
	constexpr bool const hasIndex         = hasIndexInt || hasIndexString || hasIndexFull;

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
					int isnum;
					lua_Integer number = lua_tointegerx(L, -2, &isnum);
					if (isnum)
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

	if constexpr (hasInstanceTable)
	{
		LuaHelpers::push_instance_table(L, 1);
		lua_replace(L, -4);
		lua_rawset(L, -3);
	}
	else
	{
		if constexpr (hasIndexInt && hasIndexString)
			luaL_typerror(L, 2, "integer or string");
		else if constexpr (hasIndexInt)
			luaL_typerror(L, 2, "integer");
		else if constexpr (hasIndexString)
			luaL_typerror(L, 2, "string");
	}
	return 0;
}

template <typename T>
inline bool register_newindex(lua_State* L, not_available)
{
	constexpr bool hasInstanceTable = has_init_instance_table<T>(is_available());
	constexpr bool hasIndex         = has_newindex<T>(is_available());

	if constexpr (hasInstanceTable || hasIndex)
	{
		lua_pushcfunction(L, &__newindex<T>);
		lua_setfield(L, -2, "__newindex");
		return true;
	}

	return false;
}

// *************************** PAIRS *************************

template <typename>
constexpr inline bool register_pairs(lua_State* L, not_available)
{
	return false;
}

// *************************** CALL *************************

template <typename T>
int __call(lua_State* L)
{
	T* object = reinterpret_cast<T*>(lua_touserdata(L, 1));
	return object->operator()(L);
}

template <typename T>
constexpr inline bool register_call(lua_State* L, not_available)
{
	return false;
}

template <typename T>
inline bool register_call(lua_State* L, std::enable_if_t<
                                            std::is_same_v<int, decltype(TPTR->operator()(LPTR))>,
                                            is_available>)
{
	lua_pushcfunction(L, &__call<T>);
	lua_setfield(L, -2, "__call");
	return true;
}
} // namespace

template <typename T>
T* LuaClass<T>::alloc_new(lua_State* L)
{
	T* object = reinterpret_cast<T*>(lua_newuserdata(L, sizeof(T)));

	LuaHelpers::push_class_table_container(L);
	lua_pushlightuserdata(L, object);
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	lua_pop(L, 1);

	push_metatable(L);
	lua_setmetatable(L, -2);

	if constexpr (has_init_instance_table<T>(is_available()))
	{
		LuaHelpers::push_instance_table_container(L);
		lua_pushvalue(L, -2);
		lua_newtable(L);
		__init_instance_table<T>(L, object);
		lua_rawset(L, -3);
		lua_pop(L, 1);
	}

	return object;
}

template <typename T>
T* LuaClass<T>::from_stack(lua_State* L, int idx, bool throwError)
{
	int equal = 0;

	void* p = lua_touserdata(L, idx);
	if (p != nullptr && lua_getmetatable(L, idx))
	{
		lua_getfield(L, LUA_REGISTRYINDEX, __typename<T>());
		equal = lua_rawequal(L, -1, -2);
		lua_pop(L, 2);
	}

	if (equal)
		return reinterpret_cast<T*>(p);

	if (throwError)
		luaL_typerror(L, idx, __typename<T>());

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
		lua_createtable(L, 1, 12);

		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, tname);

		lua_pushstring(L, tname);
		lua_setfield(L, -2, "__name");

		lua_pushboolean(L, true);
		lua_setfield(L, -2, "__metatable");

		if constexpr (has_init_class_table<T>(is_available()))
		{
			// Class table
			lua_createtable(L, 0, 8);
			__init_class_table<T>(L);
			lua_rawseti(L, -2, IDX_META_CLASSTABLE);
		}

		register_gc<T>(L);
		register_len<T>(L, is_available());
		register_tostring<T>(L, is_available());
		register_eq<T>(L, is_available());
		register_lt<T>(L, is_available());
		register_le<T>(L, is_available());
		register_index<T>(L, is_available());
		register_newindex<T>(L, is_available());
		register_pairs<T>(L, is_available());
		register_call<T>(L, is_available());
	}
	return 1;
}

template <typename T>
template <int (T::*X)(lua_State*)>
int LuaClass<T>::wrapped_member(lua_State* L)
{
	T* object = LuaClass<T>::from_stack(L, 1, true);
	return (object->*X)(L);
}

#endif // DECK_ASSISTANT_LUA_CLASS_HPP_
