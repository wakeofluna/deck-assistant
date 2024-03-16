#ifndef DECK_ASSISTANT_LUA_CLASS_H_
#define DECK_ASSISTANT_LUA_CLASS_H_

#include "lua_helpers.h"
#include <new>
#include <string_view>
#include <utility>

/**
 * Wrap a class with Lua compatible meta functions
 *
 * Implement the following public functions in your class to enable metafunctions:
 *
 * __gc       : void finalize(lua_State *L)
 *                callback called before destruction
 *                NOTE: you can resurrect the instance by storing it somewhere
 *                      but lua5.1 still considers the object finalized so
 *                      the finalizer will not be called again after this.
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
                  int newindex(lua_State *L)
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
 * + static constexpr bool const LUA_IS_GLOBAL = true
 *     If true, the first instance to be created is stored globally and will be reused on
 *     future uses
 *
 * @tparam T Class to curiously recurringly wrap
 */
template <typename T>
class LuaClass : public LuaHelpers
{
public:
	static char const* type_name();

	static T* push_global_instance(lua_State* L);
	static T* from_stack(lua_State* L, int idx, bool throw_error = true);
	static int push_metatable(lua_State* L);

	static T* convert_top_of_stack(lua_State* L, bool throw_error = true);
	static void create_from_table(lua_State* L, int idx);
	static void create_from_string(lua_State* L, std::string_view const& value);

	template <typename... ARGS>
	static T* create_new(lua_State* L, ARGS... args);

	template <typename... ARGS>
	static inline int push_new(lua_State* L, ARGS... args)
	{
		create_new(L, std::forward<ARGS>(args)...);
		return 1;
	}

protected:
	static void const* m_metatable_ptr;
};

template <typename T>
void const* LuaClass<T>::m_metatable_ptr = nullptr;

#endif // DECK_ASSISTANT_LUA_CLASS_H_
