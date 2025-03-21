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

#ifndef DECK_ASSISTANT_APPLICATION_H
#define DECK_ASSISTANT_APPLICATION_H

#include <filesystem>
#include <memory_resource>
#include <string_view>
#include <vector>

typedef struct lua_State lua_State;

namespace util
{
class Paths;
}

namespace LuaHelpers
{
enum class Trust : char;
}

class Application
{
public:
	Application();
	Application(Application const&) = delete;
	Application(Application&&)      = delete;
	~Application();

	Application& operator=(Application const&) = delete;
	Application& operator=(Application&&)      = delete;

	bool init(std::vector<std::string_view>&& args);
	int run();

	static void build_initial_environment(lua_State* L, util::Paths const* paths);

private:
	static void install_function_overrides(lua_State* L);
	static void build_environment_table(lua_State* L, util::Paths const* paths);
	static void process_yielded_functions(lua_State* L, long long clock);
	void reload_deckfile(lua_State* L);

private:
	lua_State* L;
	std::pmr::memory_resource* m_mem_resource;
	util::Paths* m_paths;

	std::filesystem::path m_deckfile;
	std::filesystem::file_time_type m_deckfile_mtime;
	std::size_t m_deckfile_size;
};

#endif // DECK_ASSISTANT_APPLICATION_H
