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

#include "application.h"
#include "builtins.h"
#include "deck_font.h"
#include "deck_logger.h"
#include "deck_module.h"
#include "deck_promise.h"
#include "deck_util.h"
#include "lua_helpers.h"
#include "util_paths.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_net.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <lua.hpp>
#include <string_view>
#include <thread>

namespace
{

constexpr std::size_t const k_alignment = std::max<std::size_t>({ sizeof(void*), sizeof(lua_Number), sizeof(lua_Integer) });

void* _lua_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
	std::pmr::memory_resource* mem_resource = reinterpret_cast<std::pmr::memory_resource*>(ud);

	if (ptr)
	{
		if (!nsize)
		{
			mem_resource->deallocate(ptr, osize);
			return nullptr;
		}
		else if (nsize == osize)
		{
			return ptr;
		}
		else
		{
			void* new_mem = mem_resource->allocate(nsize, k_alignment);
			if (osize > nsize)
			{
				std::memcpy(new_mem, ptr, nsize);
			}
			else
			{
				std::memcpy(new_mem, ptr, osize);
				std::memset((char*)new_mem + osize, 0, nsize - osize);
			}

			mem_resource->deallocate(ptr, osize);
			return new_mem;
		}
	}
	else
	{
		void* new_mem = mem_resource->allocate(nsize, k_alignment);
		std::memset(new_mem, 0, nsize);
		return new_mem;
	}
}

int override_print(lua_State* L)
{
	lua_pushvalue(L, lua_upvalueindex(1));
	DeckLogger* logger = DeckLogger::from_stack(L, -1);
	lua_insert(L, 1);
	return logger->call(L);
}

int override_exit(lua_State* L)
{
	int exit_code = 0;
	if (lua_type(L, 1) == LUA_TNUMBER)
		exit_code = lua_tointeger(L, 1);

	DeckModule* deck_module = DeckModule::push_global_instance(L);
	deck_module->set_exit_requested(exit_code);

	DeckLogger::lua_log_message(L, DeckLogger::Level::Info, "Application exit requested by script with code ", exit_code);

	return 0;
}

int override_loadstring(lua_State* L)
{
	std::string_view script = LuaHelpers::check_arg_string(L, 1);
	std::string_view name   = LuaHelpers::check_arg_string_or_none(L, 2);

	int trust_level_max            = lua_tointeger(L, lua_upvalueindex(1));
	int trust_level_wanted         = lua_tointeger(L, lua_upvalueindex(2));
	LuaHelpers::Trust trust_max    = static_cast<LuaHelpers::Trust>(trust_level_max);
	LuaHelpers::Trust trust_wanted = static_cast<LuaHelpers::Trust>(trust_level_wanted);

	if (int(trust_wanted) > int(trust_max))
	{
		DeckLogger::lua_log_message(L, DeckLogger::Level::Warning, "Script attempted to call loadstring with increased privileges");
		trust_wanted = trust_max;
	}

	if (LuaHelpers::load_script_inline(L, name.empty() ? nullptr : name.data(), script, trust_wanted))
		return 1;

	// Restore the original error message
	std::string_view error = LuaHelpers::get_last_error_context().message;
	lua_pushnil(L);
	lua_pushlstring(L, error.data(), error.size());
	return 2;
}

int override_loadfile(lua_State* L)
{
	std::string_view name = LuaHelpers::check_arg_string(L, 1);

	int trust_level_max            = lua_tointeger(L, lua_upvalueindex(1));
	int trust_level_wanted         = lua_tointeger(L, lua_upvalueindex(2));
	util::Paths const* paths       = reinterpret_cast<util::Paths const*>(lua_touserdata(L, lua_upvalueindex(2)));
	LuaHelpers::Trust trust_max    = static_cast<LuaHelpers::Trust>(trust_level_max);
	LuaHelpers::Trust trust_wanted = static_cast<LuaHelpers::Trust>(trust_level_wanted);

	if (int(trust_wanted) > int(trust_max))
	{
		DeckLogger::lua_log_message(L, DeckLogger::Level::Warning, "Script attempted to call loadfile with increased privileges");
		trust_wanted = trust_max;
	}

	bool const allow_home        = (trust_max != LuaHelpers::Trust::Untrusted);
	std::filesystem::path target = paths->find_data_file(name, true, allow_home, true);

	if (target.empty())
	{
		lua_pushnil(L);
		lua_pushliteral(L, "file not found");
		return 2;
	}

	if (LuaHelpers::load_script(L, target, trust_wanted))
		return 1;

	// Restore the original error message
	std::string_view error = LuaHelpers::get_last_error_context().message;
	lua_pushnil(L);
	lua_pushlstring(L, error.data(), error.size());
	return 2;
}

int override_require(lua_State* L)
{
	std::string_view name = LuaHelpers::check_arg_string(L, 1);

	int trust_level          = lua_tointeger(L, lua_upvalueindex(1));
	util::Paths const* paths = reinterpret_cast<util::Paths const*>(lua_touserdata(L, lua_upvalueindex(2)));
	LuaHelpers::Trust trust  = static_cast<LuaHelpers::Trust>(trust_level);

	// Check the package.loaded table (upvalueindex 3)
	lua_pushvalue(L, 1);
	lua_rawget(L, lua_upvalueindex(3));
	if (lua_type(L, -1) != LUA_TNIL)
		return 1;

	std::string file_name_copy;
	std::string_view file_name = name;
	if (!file_name.ends_with(".lua"))
	{
		file_name_copy.reserve(name.size() + 5);
		file_name_copy = name;
		file_name_copy.append(".lua");
		file_name = file_name_copy;
	}

	std::filesystem::path file_path;
	LuaHelpers::Trust load_trust = LuaHelpers::Trust::Untrusted;

	if (file_path.empty())
	{
		file_path = paths->find_data_file(file_name, true, false, false);
		if (!file_path.empty())
			load_trust = LuaHelpers::Trust::Untrusted;
	}
	if (file_path.empty() && trust != LuaHelpers::Trust::Untrusted)
	{
		file_path = paths->find_data_file(file_name, false, true, false);
		if (!file_path.empty())
			load_trust = LuaHelpers::Trust::Trusted;
	}
	if (file_path.empty())
	{
		file_path = paths->find_data_file(file_name, false, false, true);
		if (!file_path.empty())
			load_trust = LuaHelpers::Trust::Admin;
	}
	if (file_path.empty())
	{
		luaL_error(L, "module '%s' not found", name.data());
		return 0;
	}

	if (!LuaHelpers::load_script(L, file_path, load_trust, false))
	{
		luaL_error(L, "error loading module '%s': %s", name.data(), LuaHelpers::get_last_error_context().message.c_str());
		return 0;
	}

	if (!LuaHelpers::pcall(L, 0, 1, false))
	{
		luaL_error(L, "error loading module '%s': %s", name.data(), LuaHelpers::get_last_error_context().message.c_str());
		return 0;
	}

	if (lua_type(L, -1) != LUA_TTABLE)
	{
		luaL_error(L, "module '%s' invalid: missing return table", name.data());
		return 0;
	}

	// Duplicate value into package table
	lua_pushvalue(L, 1);
	lua_pushvalue(L, -2);
	lua_rawset(L, lua_upvalueindex(3));

	return 1;
}

} // namespace

Application::Application()
{
	m_mem_resource = new std::pmr::unsynchronized_pool_resource({ 1024, 4096 }, std::pmr::new_delete_resource());
	m_paths        = new util::Paths();

	L = lua_newstate(&_lua_alloc, m_mem_resource);
	lua_checkstack(L, 200);
	luaL_openlibs(L);

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	SDLNet_Init();
	SDL_hid_init();
	IMG_Init(0xffffffff);
	TTF_Init();

	build_environment_tables(L, m_paths);
}

Application::~Application()
{
	TTF_Quit();
	IMG_Quit();
	SDL_hid_exit();
	SDLNet_Quit();
	SDL_Quit();

	lua_close(L);
	delete m_paths;
	delete m_mem_resource;
}

bool Application::init(std::vector<std::string_view>&& args)
{
	int const oldtop = lua_gettop(L);

	std::string_view filename = (args.size() > 1) ? args[1] : "deckfile.lua";

	if (!filename.empty())
	{
		std::error_code ec;
		std::filesystem::path full_path = std::filesystem::absolute(filename, ec);
		full_path.make_preferred();

		if (!LuaHelpers::load_script(L, full_path, LuaHelpers::Trust::Trusted))
			return false;

		m_paths->set_sandbox_path(full_path.parent_path());
	}
	else
	{
		if (!LuaHelpers::load_script_inline(L, "main-window-script", builtins::main_window_script(), LuaHelpers::Trust::Admin))
			return false;

		m_paths->set_sandbox_path(".");
	}

	// XXX remove me
	lua_pushvalue(L, -1);

	if (!LuaHelpers::pcall(L, 0, 0))
		return false;

	assert(lua_gettop(L) == oldtop + 1 && "Internal stack error while loading and running script");

	// Save a copy of the new global table
	lua_getfenv(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, "ACTIVE_SCRIPT_ENV");
	// Now pop the function, it has served its purpose
	lua_pop(L, 1);

	return true;
}

int Application::run()
{
	auto const start_time = std::chrono::steady_clock::now();
	auto clock_tick       = start_time;

	DeckModule* deck_module = DeckModule::push_global_instance(L);
	int const resettop      = lua_gettop(L);

	while (!deck_module->is_exit_requested())
	{
		auto const real_clock        = std::chrono::steady_clock::now();
		lua_Integer const clock_msec = std::chrono::duration_cast<std::chrono::milliseconds>(real_clock - start_time).count();

		// Pump the system event loop
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_QUIT:
					DeckLogger::log_message(L, DeckLogger::Level::Info, "Application quit requested by system");
					deck_module->set_exit_requested(0);
					break;
				default:
					// DeckLogger::log_message(L, DeckLogger::Level::Debug, "Unhandled SDL event with type ", std::to_string(event.type));
					break;
			}
		}
		assert(lua_gettop(L) == resettop && "Application event loop handling is not stack balanced");

		// Run all connector input tick functions
		deck_module->tick_inputs(L, clock_msec);
		assert(lua_gettop(L) == resettop && "DeckModule tick_inputs function is not stack balanced");

		// Check all yielded functions
		process_yielded_functions(L, clock_msec);
		assert(lua_gettop(L) == resettop && "Yield continuation calls is not stack balanced");

		// Run script tick function
		lua_getfield(L, LUA_REGISTRYINDEX, "ACTIVE_SCRIPT_ENV");
		LuaHelpers::emit_event(L, -1, "tick", clock_msec);
		lua_pop(L, 1);

		// Run all connector output tick functions
		deck_module->tick_outputs(L);
		assert(lua_gettop(L) == resettop && "DeckModule tick_outputs function is not stack balanced");

		// Make sure we regularly clean up
		lua_gc(L, LUA_GCSTEP, 1);

		// Wait for the next cycle. Skips ticks if we can't keep up.
		auto const lower_limit = std::chrono::steady_clock::now();
		while (clock_tick < lower_limit)
			clock_tick += std::chrono::milliseconds(10);

		std::this_thread::sleep_until(clock_tick);
	}

	assert(lua_gettop(L) == resettop && "DeckModule run loop is not stack balanced");

	deck_module->shutdown(L);
	assert(lua_gettop(L) == resettop && "DeckModule shutdown function is not stack balanced");

	int exit_code = deck_module->get_exit_code();

	lua_pop(L, 1);
	return exit_code;
}

void Application::build_environment_tables(lua_State* L, util::Paths const* paths)
{
	int const oldtop  = lua_gettop(L);
	int const oldtop1 = oldtop + 1;

	install_function_overrides(L);

	assert(lua_gettop(L) == oldtop && "Application Lua init is not stack balanced");

	LuaHelpers::push_global_environment_table(L, LuaHelpers::Trust::Admin);
	build_environment_table(L, LuaHelpers::Trust::Admin, paths);
	assert(lua_gettop(L) == oldtop1 && "Application Admin environment table build is not stack balanced");
	lua_pop(L, 1);

	LuaHelpers::push_global_environment_table(L, LuaHelpers::Trust::Trusted);
	build_environment_table(L, LuaHelpers::Trust::Trusted, paths);
	assert(lua_gettop(L) == oldtop1 && "Application Trusted environment table build is not stack balanced");
	lua_pop(L, 1);

	LuaHelpers::push_global_environment_table(L, LuaHelpers::Trust::Untrusted);
	build_environment_table(L, LuaHelpers::Trust::Untrusted, paths);
	assert(lua_gettop(L) == oldtop1 && "Application Untrusted environment table build is not stack balanced");
	lua_pop(L, 1);

	// Now the environment tables are set up, load internal scripts
	if (!LuaHelpers::load_script_inline(L, "builtins", builtins::builtins_script(), LuaHelpers::Trust::Admin))
		assert(false && "Error loading builtins script");

	for (LuaHelpers::Trust trust : { LuaHelpers::Trust::Untrusted, LuaHelpers::Trust::Trusted, LuaHelpers::Trust::Admin })
	{
		LuaHelpers::push_global_environment_table(L, trust);
		lua_getfield(L, -1, "package");
		lua_getfield(L, -1, "loaded");
		lua_pushvalue(L, -4);
		assert(LuaHelpers::pcall(L, 0, 1) && "Error running builtins script");
		assert(lua_type(L, -1) == LUA_TTABLE && "Builtins script did not return a table");
		lua_setfield(L, -2, "builtins");
		lua_pop(L, 3);
	}

	lua_pop(L, 1);
}

void Application::install_function_overrides(lua_State* L)
{
	DeckLogger::push_new(L);
	lua_pushcclosure(L, &override_print, 1);
	lua_setglobal(L, "print");

	lua_getglobal(L, "os");
	if (lua_type(L, -1) == LUA_TTABLE)
	{
		lua_pushcfunction(L, &override_exit);
		lua_setfield(L, -2, "exit");
	}
	lua_pop(L, 1);
}

void Application::build_environment_table(lua_State* L, LuaHelpers::Trust trust, util::Paths const* paths)
{
	// Copy functions that are allowed for all clients
	char const* const untrusted_keys[] = {
		// basic
		"_VERSION",
		"assert",
		"error",
		"ipairs",
		"next",
		"pairs",
		"pcall",
		"print",
		"rawequal",
		"select",
		"tonumber",
		"tostring",
		"type",
		"unpack",
		"xpcall",
	};

	for (auto key : untrusted_keys)
	{
		lua_getglobal(L, key);
		lua_setfield(L, -2, key);
	}

	// Deep copy the module tables
	for (auto module : { "coroutine", "math", "string", "table" })
	{
		lua_createtable(L, 0, 32);
		lua_getglobal(L, module);

		if (lua_type(L, -1) == LUA_TTABLE)
		{
			LuaHelpers::copy_table_fields(L);
			lua_setfield(L, -2, module);
		}
		else
		{
			lua_pop(L, 2);
		}
	}

	// Only allow some fields from the OS module
	lua_getglobal(L, "os");
	if (lua_type(L, -1) == LUA_TTABLE)
	{
		lua_createtable(L, 0, 5);
		for (auto key : { "clock", "date", "difftime", "exit", "time" })
		{
			lua_pushstring(L, key);
			lua_pushvalue(L, -1);
			lua_gettable(L, -4);
			lua_settable(L, -3);
		}
		lua_setfield(L, -3, "os");
	}
	lua_pop(L, 1);

	// Create the package table
	if (trust == LuaHelpers::Trust::Admin)
	{
		lua_getglobal(L, "package");
		lua_setfield(L, -2, "package");
	}
	else
	{
		lua_createtable(L, 0, 1);
		lua_createtable(L, 0, 16);
		for (auto module : { "coroutine", "math", "string", "table", "os" })
		{
			lua_getfield(L, -3, module);
			lua_setfield(L, -2, module);
		}
		lua_setfield(L, -2, "loaded");
		lua_setfield(L, -2, "package");
	}

	// Add our API to the package table
	lua_getfield(L, -1, "package");
	lua_getfield(L, -1, "loaded");
	DeckModule::push_new(L);
	lua_setfield(L, -2, "deck");
	DeckLogger::push_new(L);
	lua_setfield(L, -2, "deck.logger");
	DeckUtil::push_new(L, trust, paths);
	lua_setfield(L, -2, "deck.util");
	lua_pop(L, 2);

	DeckFont::insert_enum_values(L);

	// Build script loader functions
	lua_pushinteger(L, int(trust));
	lua_pushinteger(L, int(LuaHelpers::Trust::Untrusted));
	lua_pushcclosure(L, &override_loadstring, 2);
	lua_setfield(L, -2, "loadstring");

	lua_pushinteger(L, int(trust));
	lua_pushinteger(L, int(LuaHelpers::Trust::Trusted));
	lua_pushcclosure(L, &override_loadstring, 2);
	lua_setfield(L, -2, "loadstring_trusted");

	lua_pushinteger(L, int(trust));
	lua_pushinteger(L, int(LuaHelpers::Trust::Admin));
	lua_pushcclosure(L, &override_loadstring, 2);
	lua_setfield(L, -2, "loadstring_admin");

	lua_pushinteger(L, int(trust));
	lua_pushinteger(L, int(LuaHelpers::Trust::Untrusted));
	lua_pushlightuserdata(L, (void*)paths);
	lua_pushcclosure(L, &override_loadfile, 3);
	lua_setfield(L, -2, "loadfile");

	lua_pushinteger(L, int(trust));
	lua_pushinteger(L, int(LuaHelpers::Trust::Trusted));
	lua_pushlightuserdata(L, (void*)paths);
	lua_pushcclosure(L, &override_loadfile, 3);
	lua_setfield(L, -2, "loadfile_trusted");

	lua_pushinteger(L, int(trust));
	lua_pushinteger(L, int(LuaHelpers::Trust::Admin));
	lua_pushlightuserdata(L, (void*)paths);
	lua_pushcclosure(L, &override_loadfile, 3);
	lua_setfield(L, -2, "loadfile_admin");

	lua_pushinteger(L, int(trust));
	lua_pushlightuserdata(L, (void*)paths);
	lua_getfield(L, -3, "package");
	lua_getfield(L, -1, "loaded");
	lua_replace(L, -2);
	lua_pushcclosure(L, &override_require, 3);
	lua_setfield(L, -2, "require");

	if (trust == LuaHelpers::Trust::Untrusted)
		return;

	// Functions for at-least Trusted

	char const* const trusted_keys[] = {
		// basic
		"collectgarbage",
		"getfenv",
		"getmetatable",
		"rawget",
		"rawset",
		"setfenv",
		"setmetatable",
	};

	for (auto key : trusted_keys)
	{
		lua_getglobal(L, key);
		lua_setfield(L, -2, key);
	}

	if (trust == LuaHelpers::Trust::Trusted)
		return;

	// Functions for at-least Admin

	// Modules are fully accessible instead of copies
	for (auto module : { "coroutine", "debug", "io", "math", "os", "string", "table" })
	{
		lua_getglobal(L, module);
		lua_setfield(L, -2, module);
	}

	return;
}

void Application::process_yielded_functions(lua_State* L, long long clock)
{
	LuaHelpers::push_yielded_calls_table(L);

	lua_pushnil(L);
	while (lua_next(L, -2))
	{
		lua_State* thread = lua_tothread(L, -2);
		assert(thread != nullptr && "Non-thread in yielded calls table");
		assert(lua_status(thread) == LUA_YIELD && "Non-yielded thread in yielded calls table");

		if (DeckPromise* promise = DeckPromise::from_stack(L, -1, false); promise)
		{
			if (!promise->check_wakeup(clock))
			{
				lua_pop(L, 1);
				continue;
			}

			LuaHelpers::push_instance_table(L, -1);
			lua_pushliteral(L, "value");
			lua_rawget(L, -2);
			lua_xmove(L, thread, 1);
			lua_insert(thread, 1);
			lua_pop(L, 1);
		}

		lua_pop(L, 1);

		int result = lua_resume(thread, lua_gettop(thread));
		if (result == LUA_YIELD)
		{
			lua_pushvalue(L, -1);
			if (lua_isnoneornil(thread, 1))
			{
				lua_pushboolean(L, true);
			}
			else
			{
				lua_pushvalue(thread, 1);
				lua_xmove(thread, L, 1);
			}
			lua_rawset(L, -4);
		}
		else
		{
			if (result != LUA_OK)
			{
				std::string_view message = LuaHelpers::to_string_view(thread, -1);
				DeckLogger::log_message(thread, DeckLogger::Level::Error, message);
			}
			lua_pushvalue(L, -1);
			lua_pushnil(L);
			lua_rawset(L, -4);
		}
	}

	lua_pop(L, 1);
}
