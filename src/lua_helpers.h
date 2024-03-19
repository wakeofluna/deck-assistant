#ifndef DECK_ASSISTANT_LUA_HELPERS_H
#define DECK_ASSISTANT_LUA_HELPERS_H

#include <iosfwd>
#include <lua.hpp>
#include <string>
#include <string_view>

// Compatibility between luajit and lua5.1
#ifndef LUA_OK
#define LUA_OK 0
#endif

// Indices in class metatable
constexpr lua_Integer const IDX_META_CLASSTABLE      = 1;
constexpr lua_Integer const IDX_META_GLOBAL_INSTANCE = 2;

struct LuaHelpers
{
	struct ErrorContext
	{
		void clear();

		int result;
		std::string message;
		std::string source_name;
		int line;
	};

	static int absidx(lua_State* L, int idx);

	static void push_standard_weak_key_metatable(lua_State* L);
	static void push_standard_weak_value_metatable(lua_State* L);

	static void push_class_table(lua_State* L, int idx);
	static void push_instance_table(lua_State* L, int idx);

	static std::string_view check_arg_string(lua_State* L, int idx, bool allow_empty = false);
	static std::string_view check_arg_string_or_none(lua_State* L, int idx);
	static lua_Integer check_arg_int(lua_State* L, int idx);

	static std::string_view push_converted_to_string(lua_State* L, int idx);

	static void newindex_store_in_instance_table(lua_State* L);
	static void copy_table_fields(lua_State* L);

	static bool load_script(lua_State* L, char const* file_name, bool log_error = true);
	static bool load_script_inline(lua_State* L, char const* chunk_name, std::string_view const& script, bool log_error = true);
	static void assign_new_env_table(lua_State* L, int idx, char const* chunk_name);

	static bool pcall(lua_State* L, int nargs, int nresults, bool log_error = true);
	static bool lua_lineinfo(lua_State* L, std::string& short_src, int& currentline);

	static void install_error_context_handler(lua_State* L);
	static ErrorContext const& get_last_error_context();

	static void debug_dump_stack(std::ostream& stream, lua_State* L, char const* description = nullptr);
	static void debug_dump_stack(lua_State* L, char const* description = nullptr);

	static void debug_dump_table(std::ostream& stream, lua_State* L, int idx, bool recursive = false, char const* description = nullptr);
	static void debug_dump_table(lua_State* L, int idx, bool recursive = false, char const* description = nullptr);
};

#endif // DECK_ASSISTANT_LUA_HELPERS_H
