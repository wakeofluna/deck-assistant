#include "deck_logger.h"
#include "lua_class.hpp"
#include <chrono>
#include <format>
#include <iostream>
#include <sstream>

template class LuaClass<DeckLogger>;

namespace
{

constexpr char const g_DEBUG   = char(DeckLogger::Level::Debug);
constexpr char const g_INFO    = char(DeckLogger::Level::Info);
constexpr char const g_WARNING = char(DeckLogger::Level::Warning);
constexpr char const g_ERROR   = char(DeckLogger::Level::Error);

void push_level(lua_State* L, DeckLogger::Level level)
{
	switch (level)
	{
		case DeckLogger::Level::Debug:
			lua_pushlightuserdata(L, (void*)&g_DEBUG);
			break;
		case DeckLogger::Level::Info:
			lua_pushlightuserdata(L, (void*)&g_INFO);
			break;
		case DeckLogger::Level::Warning:
			lua_pushlightuserdata(L, (void*)&g_WARNING);
			break;
		case DeckLogger::Level::Error:
			lua_pushlightuserdata(L, (void*)&g_ERROR);
			break;
	}
}

} // namespace

char const* DeckLogger::LUA_TYPENAME          = "deck-assistant.DeckLogger";
char const* DeckLogger::LUA_GLOBAL_INDEX_NAME = "DeckLoggerInstance";

DeckLogger::DeckLogger()
    : m_block_logs(false)
{
}

void DeckLogger::log_message(lua_State* L, Level level, std::string_view const& message) const
{
	if (message.empty())
		return;

	stream_output(std::cout, level, message);

	if (!m_block_logs)
	{
		m_block_logs = true;

		int const resettop = lua_gettop(L);

		push_this(L);
		push_instance_table(L, -1);
		lua_getfield(L, -1, "on_message");
		if (lua_type(L, -1) == LUA_TFUNCTION)
		{
			push_level(L, level);
			lua_pushlstring(L, message.data(), message.size());
			if (!LuaHelpers::pcall(L, 2, 0))
			{
				std::stringstream buf;
				buf << "Additionally, an error occured in the Logger on_message callback: ";
				LuaHelpers::print_error_context(buf);

				stream_output(std::cerr, Level::Error, buf.str());
			}
		}

		lua_settop(L, resettop);

		m_block_logs = false;
	}
}

void DeckLogger::init_class_table(lua_State* L)
{
	push_level(L, Level::Debug);
	lua_setfield(L, -2, "DEBUG");

	push_level(L, Level::Info);
	lua_setfield(L, -2, "INFO");

	push_level(L, Level::Warning);
	lua_setfield(L, -2, "WARNING");

	push_level(L, Level::Error);
	lua_setfield(L, -2, "ERROR");
}

void DeckLogger::init_instance_table(lua_State* L)
{
}

int DeckLogger::newindex(lua_State* L)
{
	if (lua_type(L, 2) == LUA_TSTRING)
	{
		std::string_view key = check_arg_string(L, 2);
		if (key == "on_message")
		{
			luaL_argcheck(L, lua_type(L, 3) == LUA_TFUNCTION, 3, "must be a function");
			newindex_store_in_instance_table(L);
			return 0;
		}
	}

	luaL_argerror(L, absidx(L, 2), "invalid key for DeckLogger (allowed: on_message)");
	return 0;
}

int DeckLogger::call(lua_State* L)
{
	return _lua_logger(L);
}

void DeckLogger::stream_output(std::ostream& out, Level level, std::string_view const& message)
{
	auto now_fp = std::chrono::system_clock::now();
	auto now    = std::chrono::round<std::chrono::milliseconds>(now_fp);

	std::string_view level_str;
	switch (level)
	{
		case Level::Debug: level_str = "[DEBUG] "; break;
		case Level::Info: break;
		case Level::Warning: level_str = "[WARNING] "; break;
		case Level::Error: level_str = "[ERROR] "; break;
	}

	std::string prefix = std::format("[{0:%F} {0:%T}] {1}", now, level_str);

	std::size_t last_offset = message.size();
	while (last_offset > 0 && message[last_offset - 1] <= 32)
		--last_offset;

	std::string_view msg = message.substr(0, last_offset);

	std::size_t offset = 0;
	while (offset < last_offset)
	{
		std::size_t found = msg.find('\n', offset);

		std::string_view line;

		if (found == std::string_view::npos)
		{
			line   = msg.substr(offset);
			offset = last_offset;
		}
		else
		{
			line   = msg.substr(offset, found - offset);
			offset = found + 1;
		}

		size_t trim_offset = line.size();
		while (trim_offset > 0 && line[trim_offset - 1] <= 32)
			--trim_offset;

		out << prefix << line.substr(0, trim_offset) << '\n';
	}
}

int DeckLogger::_lua_logger(lua_State* L)
{
	DeckLogger* self = from_stack(L, 1);

	Level level  = Level::Info;
	int next_idx = 2;

	if (lua_type(L, next_idx) == LUA_TLIGHTUSERDATA)
	{
		bool valid_level = true;

		void const* ud = lua_touserdata(L, 2);
		if (ud == &g_DEBUG)
			level = Level::Debug;
		else if (ud == &g_INFO)
			level = Level::Info;
		else if (ud == &g_WARNING)
			level = Level::Warning;
		else if (ud == &g_ERROR)
			level = Level::Error;
		else
			valid_level = false;

		if (valid_level)
			++next_idx;
	}

	std::string message;
	bool first = true;

	while (lua_type(L, next_idx) != LUA_TNONE)
	{
		if (first)
			first = false;
		else
			message += ' ';

		std::string_view item  = LuaHelpers::push_converted_to_string(L, next_idx);
		message               += item;
		lua_pop(L, 1);

		++next_idx;
	}

	self->log_message(L, level, message);

	return 0;
}
