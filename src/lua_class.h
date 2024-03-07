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
 * __gc       : bool finalize(lua_State *L)
 *                chance to cancel collection by creating a new reference
 *                return true to call C++ destructor or false to bypass destructor
 * __len      : int length() const
 * __tostring : For full control:
 *                int to_string(lua_State *L) const
 *              Or convenience function:
 *                const char * to_string() const
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
 * Additionally the class supports two tables:
 * + Class table: shared between all instances, for functions and default values
 *   Enable (and optionally pre-fill) the class table by implementing:
 *     static void init_class_table(lua_State *L)
 * + Instance table: private per instance
 *   Enable (and optionally pre-fill) the instance table by implementing:
 *     void init_instance_table(lua_State *L)
 *
 * @tparam T Class to curiously recurringly wrap
 */
template <typename T>
class LuaClass : public LuaHelpers
{
public:
	static char const* type_name();
	static T* from_stack(lua_State* L, int idx, bool throw_error = true);
	static bool is_on_stack(lua_State* L, int idx);
	static int push_metatable(lua_State* L);
	static void convert_top_of_stack(lua_State* L);

	template <typename... ARGS>
	static T* create_new(lua_State* L, ARGS... args);

	template <typename... ARGS>
	static inline int push_new(lua_State* L, ARGS... args)
	{
		create_new(L, std::forward<ARGS>(args)...);
		return 1;
	}

protected:
	template <int (T::*X)(lua_State*)>
	static int wrapped_member(lua_State* L);
};

#endif // DECK_ASSISTANT_LUA_CLASS_H_
