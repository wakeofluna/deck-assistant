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

#include "deck_logger.h"
#include "lua_helpers.h"
#include <chrono>
#include <format>
#include <iostream>

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

bool to_level(lua_State* L, int idx, DeckLogger::Level& level)
{
	bool valid_level = true;
	void const* ud   = lua_touserdata(L, idx);
	if (ud == &g_DEBUG)
		level = DeckLogger::Level::Debug;
	else if (ud == &g_INFO)
		level = DeckLogger::Level::Info;
	else if (ud == &g_WARNING)
		level = DeckLogger::Level::Warning;
	else if (ud == &g_ERROR)
		level = DeckLogger::Level::Error;
	else
		valid_level = false;
	return valid_level;
}

} // namespace

char const* DeckLogger::LUA_TYPENAME = "deck:DeckLogger";

DeckLogger::Level DeckLogger::m_min_level = DeckLogger::Level::Info;

DeckLogger::DeckLogger()
    : m_block_logs(false)
{
}

void DeckLogger::override_min_level(Level level)
{
	m_min_level = level;
}

void DeckLogger::log_message(lua_State* L, Level level, std::string_view const& message)
{
	if (message.empty())
		return;

	if (int(level) < int(m_min_level))
		return;

	stream_output(std::cout, level, message);

	if (!L)
		return;

	DeckLogger* logger = push_global_instance(L);
	if (logger && !logger->m_block_logs)
	{
		logger->m_block_logs = true;
		int const resettop   = lua_gettop(L);

		LuaHelpers::push_instance_table(L, -1);
		lua_getfield(L, -1, "on_message");
		if (lua_type(L, -1) == LUA_TFUNCTION)
		{
			push_level(L, level);
			lua_pushlstring(L, message.data(), message.size());
			if (lua_pcall(L, 2, 0, 0) != LUA_OK)
			{
				std::string buf;
				buf.reserve(256);
				buf  = "Additionally, an error occured in the Logger on_message callback:\n";
				buf += LuaHelpers::to_string_view(L, -1);
				stream_output(std::cerr, Level::Error, buf);
				lua_pop(L, 1);
			}
		}

		lua_settop(L, resettop);
		logger->m_block_logs = false;
	}
	lua_pop(L, 1);
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
	push_level(L, m_min_level);
	lua_setfield(L, -2, "min_level");
}

int DeckLogger::newindex(lua_State* L)
{
	if (lua_type(L, 2) == LUA_TSTRING)
	{
		std::string_view key = LuaHelpers::to_string_view(L, 2);
		if (key == "on_message")
		{
			luaL_argcheck(L, lua_type(L, 3) == LUA_TFUNCTION, 3, "must be a function");
			LuaHelpers::newindex_store_in_instance_table(L);
			return 0;
		}
		if (key == "min_level")
		{
			if (!to_level(L, 3, m_min_level))
				luaL_argerror(L, 3, "not a valid loglevel");
			LuaHelpers::newindex_store_in_instance_table(L);
			return 0;
		}
	}

	luaL_argerror(L, LuaHelpers::absidx(L, 2), "invalid key for DeckLogger (allowed: on_message, min_level)");
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
		if (to_level(L, next_idx, level))
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
