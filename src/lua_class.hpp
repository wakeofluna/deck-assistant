#ifndef DECK_ASSISTANT_LUA_CLASS_HPP_
#define DECK_ASSISTANT_LUA_CLASS_HPP_

#include "lua_class.h"
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

// *************************** GLOBAL INDEX *************************

template <typename T>
constexpr inline bool has_global_index_name(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_global_index_name(std::enable_if_t<
                                            std::is_same_v<char const*, decltype(T::LUA_GLOBAL_INDEX_NAME)>,
                                            is_available>)
{
	return true;
}

template <typename T>
constexpr inline bool has_global_index_name()
{
	return has_global_index_name<T>(is_available());
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
int __gc(lua_State* L)
{
	T* object = reinterpret_cast<T*>(lua_touserdata(L, 1));
	if constexpr (has_finalize<T>(is_available()))
		object->finalize(L);

	object->~T();
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
template <typename... ARGS>
T* LuaClass<T>::create_new(lua_State* L, ARGS... args)
{
	if constexpr (has_global_index_name<T>())
	{
		push_global_instance(L);
		assert(lua_type(L, -1) == LUA_TNIL && "global instance of type already exists!");
		lua_pop(L, 1);
	}

	T* object = reinterpret_cast<T*>(lua_newuserdata(L, sizeof(T)));
	new (object) T(std::forward<ARGS>(args)...);

	LuaHelpers::push_userdata_container(L);
	lua_pushlightuserdata(L, object);
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	lua_pop(L, 1);

	push_metatable(L);
	lua_setmetatable(L, -2);

	if constexpr (has_global_index_name<T>())
	{
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, T::LUA_GLOBAL_INDEX_NAME);
	}

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
void LuaClass<T>::push_global_instance(lua_State* L)
{
	if constexpr (has_global_index_name<T>())
	{
		lua_getfield(L, LUA_REGISTRYINDEX, T::LUA_GLOBAL_INDEX_NAME);
	}
	else
	{
		assert(false && "attempted to get global instance of non-global class");
	}
}

template <typename T>
T* LuaClass<T>::get_global_instance(lua_State* L)
{
	push_global_instance(L);
	T* instance = from_stack(L, -1, false);
	lua_pop(L, 1);
	return instance;
}

template <typename T>
T* LuaClass<T>::from_stack(lua_State* L, int idx, bool throw_error)
{
	lua_getmetatable(L, idx);

	bool ok = (lua_type(L, -1) == LUA_TTABLE && lua_topointer(L, -1) == T::m_metatable_ptr);
	lua_pop(L, 1);

	if (ok)
		return reinterpret_cast<T*>(lua_touserdata(L, idx));

	if (throw_error)
	{
		idx = absidx(L, idx);
		luaL_typerror(L, idx, __typename<T>());
	}

	return nullptr;
}

template <typename T>
bool LuaClass<T>::is_on_stack(lua_State* L, int idx)
{
	lua_getmetatable(L, idx);

	bool ok = (lua_type(L, -1) == LUA_TTABLE && lua_topointer(L, -1) == T::m_metatable_ptr);
	lua_pop(L, 1);

	return ok;
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
		register_tostring<T>(L, is_available());
		register_eq<T>(L, is_available());
		register_lt<T>(L, is_available());
		register_le<T>(L, is_available());
		register_index<T>(L, is_available());
		register_newindex<T>(L, is_available());
		register_pairs<T>(L, is_available());
		register_call<T>(L, is_available());

		T::m_metatable_ptr = lua_topointer(L, -1);
	}
	return 1;
}

template <typename T>
constexpr inline bool has_simple_constructor(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_simple_constructor(std::enable_if_t<
                                             std::is_same_v<T, decltype(T())>,
                                             is_available>)
{
	return true;
}

template <typename T>
constexpr inline bool has_string_constructor(not_available)
{
	return false;
}

template <typename T>
constexpr inline bool has_string_constructor(std::enable_if_t<
                                             std::is_same_v<T, decltype(T("hello"))>,
                                             is_available>)
{
	return true;
}

template <typename T>
T* LuaClass<T>::convert_top_of_stack(lua_State* L, bool throw_error)
{
	int const vtype    = lua_type(L, -1);
	int const checktop = lua_gettop(L);

	if (vtype == LUA_TNIL)
	{
		if constexpr (has_simple_constructor<T>(is_available()))
		{
			T::push_new(L);
		}
	}
	else if (vtype == LUA_TSTRING)
	{
		size_t len;
		char const* str = lua_tolstring(L, -1, &len);
		std::string_view value(str, len);

		T::create_from_string(L, value);
	}
	else if (vtype == LUA_TTABLE)
	{
		T::create_from_table(L, -1);
	}

	assert(lua_gettop(L) >= checktop);
	assert(lua_type(L, checktop) == vtype);

	T* item = from_stack(L, -1, false);
	if (item)
		lua_replace(L, checktop);

	lua_settop(L, checktop);

	if (!item && throw_error)
		luaL_error(L, "unable to auto-convert from %s to %s", lua_typename(L, vtype), __typename<T>());

	return item;
}

template <typename T>
void LuaClass<T>::create_from_table(lua_State* L, int idx)
{
	if constexpr (has_simple_constructor<T>(is_available()))
	{
		assert(lua_type(L, idx) == LUA_TTABLE && "create_from_table called on a non-table value");
		idx = absidx(L, idx);

		T::push_new(L);
		lua_pushvalue(L, idx);
		copy_table_fields(L);
	}
}

template <typename T>
void LuaClass<T>::create_from_string(lua_State* L, std::string_view const& value)
{
	if constexpr (has_string_constructor<T>(is_available()))
	{
		T::push_new(L, value);
	}
}

#endif // DECK_ASSISTANT_LUA_CLASS_HPP_
