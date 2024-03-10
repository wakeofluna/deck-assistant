#ifndef DECK_ASSISTANT_LUA_HELPERS_H
#define DECK_ASSISTANT_LUA_HELPERS_H

#include <iosfwd>
#include <lua.hpp>
#include <string>
#include <string_view>
#include <vector>

// Indices in class metatable
constexpr lua_Integer IDX_META_CLASSTABLE = 1;

struct LuaHelpers
{
	struct ErrorContext
	{
		struct Frame
		{
			std::string source_name;
			std::string function_name;
			int line_defined;
			int line;
		};

		int call_result;
		std::string message;
		std::vector<Frame> stack;

		void reset();
	};

	void push_this(lua_State* L) const;

	static int absidx(lua_State* L, int idx);

	static void push_standard_weak_key_metatable(lua_State* L);
	static void push_standard_weak_value_metatable(lua_State* L);

	static void push_userdata_container(lua_State* L);
	static void push_instance_table_container(lua_State* L);

	static void push_class_table(lua_State* L, int idx);
	static void push_instance_table(lua_State* L, int idx);

	static void* check_arg_userdata(lua_State* L, int idx, char const* tname, bool throw_error = true);
	static std::string_view check_arg_string(lua_State* L, int idx, bool allow_empty = false);
	static std::string_view check_arg_string_or_none(lua_State* L, int idx);
	static lua_Integer check_arg_int(lua_State* L, int idx);

	static std::string_view push_converted_to_string(lua_State* L, int idx);

	static void newindex_store_in_instance_table(lua_State* L);
	static void newindex_type_or_convert(lua_State* L, char const* tname, lua_CFunction type_create, char const* text_field);
	static void copy_table_fields(lua_State* L);

	static bool load_script(lua_State* L, char const* file_name);
	static bool load_script_inline(lua_State* L, char const* chunk_name, std::string_view const& script);
	static void assign_new_env_table(lua_State* L, int idx, char const* chunk_name);

	static bool pcall(lua_State* L, int nargs, int nresults);
	static ErrorContext const& get_last_error_context();
	static void print_error_context(std::ostream& stream, ErrorContext const& context);
	static inline void print_error_context(std::ostream& stream) { print_error_context(stream, get_last_error_context()); }

	static void debug_dump_stack(std::ostream& stream, lua_State* L, char const* description = nullptr);
};

#endif // DECK_ASSISTANT_LUA_HELPERS_H
