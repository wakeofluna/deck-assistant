#ifndef DECK_ASSISTANT_DECK_LOGGER_H
#define DECK_ASSISTANT_DECK_LOGGER_H

#include "lua_class.h"
#include <iosfwd>

class DeckLogger : public LuaClass<DeckLogger>
{
public:
	enum class Level : char
	{
		Debug,
		Info,
		Warning,
		Error,
	};

public:
	static char const* LUA_TYPENAME;
	static constexpr bool const LUA_IS_GLOBAL = true;

	DeckLogger();

	static void log_message(lua_State* L, Level level, std::string_view const& message);
	static void lua_log_message(lua_State* L, Level level, std::string_view const& message);
	static void lua_log_message(lua_State* L, Level level, std::string_view const& message, std::string_view const& part2);

	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int newindex(lua_State* L);
	int call(lua_State* L);

private:
	static void stream_output(std::ostream& out, Level level, std::string_view const& message);
	static int _lua_logger(lua_State* L);

private:
	mutable bool m_block_logs;
};

#endif // DECK_ASSISTANT_DECK_LOGGER_H
