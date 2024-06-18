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

#ifndef DECK_ASSISTANT_LUA_CLASS_H_
#define DECK_ASSISTANT_LUA_CLASS_H_

#include <lua.hpp>
#include <utility>

/**
 * Wrap a class with Lua compatible meta functions
 *
 * Implement the following public functions in your class to enable metafunctions:
 *
 * __gc       : void finalize(lua_State *L)
 *                callback called before destruction
 *                NOTE: you could in theory resurrect the instance by storing it
 *                      somewhere but lua5.1 still considers the object finalized
 *                      so the finalizer will not be called again after this
 *                      AND the underlying C++ object will be destroyed anyway!
 * __tostring : For full control:
 *                int tostring(lua_State *L) const
 *              Or convenience function:
 *                const char * tostring() const
 * __eq       : bool operator== (const T&) const
 * __lt       : bool operator<  (const T&) const
 * __le       : bool operator<= (const T&) const
 * __index    : For full control:
 *                int index(lua_State *L)
 *              Or convenience functions:
 *                int index(lua_State *L, lua_Integer key)
 *                int index(lua_State *L, std::string_view const& key)
 * __newindex : For full control:
 *                int newindex(lua_State *L)
 *              Or convenience functions:
 *                int newindex(lua_State *L, lua_Integer key)
 *                int newindex(lua_State *L, std::string_view const& key)
 * __call     : int call(lua_State *L)
 *
 *
 * Additionally the class supports two tables:
 *
 * + Class table: shared between all instances, for functions and default values
 *   Enable (and optionally pre-fill) the class table by implementing:
 *     static void init_class_table(lua_State *L)
 * + Instance table: private per instance
 *   Enable (and optionally pre-fill) the instance table by implementing:
 *     void init_instance_table(lua_State *L)
 *
 *
 * Finally, you can set some static constants to control some aspects of the class
 *
 * + static char const* LUA_TYPENAME
 *     Use this string as the name of the metatable and classname in print commands
 * + static constexpr bool const LUA_ENABLE_PUSH_THIS = true
 *     If true, keeps track of a mapping of pointers to userdata
 * + static constexpr bool const LUA_IS_GLOBAL = true
 *     If true, the first instance to be created is stored globally and will be reused on
 *     future uses. Implies LUA_ENABLE_PUSH_THIS = true
 *
 * @tparam T Class to curiously recurringly wrap
 */
template <typename T>
class LuaClass
{
public:
	static char const* type_name();

	static void push_instance_list_table(lua_State* L);
	void push_this(lua_State* L);
	inline int get_lua_ref_id() const { return m_lua_ref_id; }
	void set_lua_ref_id(int ref_id);

	static T* push_global_instance(lua_State* L);
	static T* push_from_ref_id(lua_State* L, int lua_ref_id);
	static T* from_stack(lua_State* L, int idx, bool throw_error = true);
	static int push_metatable(lua_State* L);

	template <typename... ARGS>
	static T* push_new(lua_State* L, ARGS&&... args)
	{
		T* object = check_global_instance(L);
		if (!object)
		{
			object = reinterpret_cast<T*>(lua_newuserdata(L, sizeof(T)));
			new (object) T(std::forward<ARGS>(args)...);
			finish_initialisation(L, object);
		}
		return object;
	}

private:
	static T* check_global_instance(lua_State* L);
	static void finish_initialisation(lua_State* L, T* object);

	static void const* m_metatable_ptr;
	int m_lua_ref_id;
};

template <typename T>
void const* LuaClass<T>::m_metatable_ptr = nullptr;

#endif // DECK_ASSISTANT_LUA_CLASS_H_
