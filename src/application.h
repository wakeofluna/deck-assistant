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

#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

typedef struct lua_State lua_State;

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

	static void build_environment_tables(lua_State* L);

private:
	static void install_function_overrides(lua_State* L);
	static void build_environment_table(lua_State* L, LuaHelpers::Trust trust);
	static void process_yielded_functions(lua_State* L, long long clock);

private:
	lua_State* L;
	std::pmr::memory_resource* m_mem_resource;
	std::string m_deckfile_file_name;
};

#endif // DECK_ASSISTANT_APPLICATION_H
