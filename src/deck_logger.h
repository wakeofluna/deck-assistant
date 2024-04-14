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

#ifndef DECK_ASSISTANT_DECK_LOGGER_H
#define DECK_ASSISTANT_DECK_LOGGER_H

#include "lua_class.h"
#include "lua_helpers.h"
#include <iosfwd>
#include <sstream>
#include <string>
#include <cwchar>
#include <vector>

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

	// For testing
	static void override_min_level(Level level);

	static void log_message(lua_State* L, Level level, std::string_view const& message);

	template <typename T, typename... ARGS>
	static void log_message(lua_State* L, Level level, std::string_view const& message, T arg, ARGS... args);

	template <typename... ARGS>
	static void lua_log_message(lua_State* L, Level level, std::string_view const& message, ARGS... args);

	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int newindex(lua_State* L);
	int call(lua_State* L);

private:
	static void stream_output(std::ostream& out, Level level, std::string_view const& message);
	static int _lua_logger(lua_State* L);

	template <typename T, typename... ARGS>
	static inline void _string_stream(std::stringstream& sstream, T value, ARGS... args);

	template <typename... ARGS>
	static inline void _string_stream(std::stringstream& sstream, wchar_t const* value, ARGS... args);

private:
	mutable bool m_block_logs;
	static Level m_min_level;
};

template <typename T, typename... ARGS>
inline void DeckLogger::_string_stream(std::stringstream& sstream, T value, ARGS... args)
{
	sstream << value;
	if constexpr (sizeof...(args) > 0)
		_string_stream(sstream, std::forward<ARGS>(args)...);
}

template <typename... ARGS>
inline void DeckLogger::_string_stream(std::stringstream& sstream, wchar_t const* value, ARGS... args)
{
	std::mbstate_t state = std::mbstate_t();
	std::size_t len = 1 + std::wcsrtombs(nullptr, &value, 0, &state);
	std::vector<char> mbstr(len);
	std::size_t size = std::wcsrtombs(&mbstr[0], &value, mbstr.size(), &state);

	if (size != std::size_t(-1))
		sstream.write(mbstr.data(), size);

	if constexpr (sizeof...(args) > 0)
		_string_stream(sstream, std::forward<ARGS>(args)...);
}

template <typename T, typename... ARGS>
void DeckLogger::log_message(lua_State* L, Level level, std::string_view const& message, T arg, ARGS... args)
{
	if (int(level) < int(m_min_level))
		return;

	std::string full_message;
	full_message.reserve(256);

	std::stringstream stream(std::move(full_message));
	stream << message;
	_string_stream(stream, std::forward<T>(arg), std::forward<ARGS>(args)...);

	log_message(L, level, std::move(stream).str());
}

template <typename... ARGS>
void DeckLogger::lua_log_message(lua_State* L, Level level, std::string_view const& message, ARGS... args)
{
	if (int(level) < int(m_min_level))
		return;

	std::string full_message;
	full_message.reserve(256);

	int currentline;
	bool const line_ok = LuaHelpers::lua_lineinfo(L, full_message, currentline);

	std::stringstream stream(std::move(full_message), std::ios::in | std::ios::out | std::ios::app);
	if (line_ok)
		stream << ':' << currentline << ": ";

	stream << message;

	if constexpr (sizeof...(args) > 0)
		_string_stream(stream, std::forward<ARGS>(args)...);

	log_message(L, level, std::move(stream).str());
}

#endif // DECK_ASSISTANT_DECK_LOGGER_H
