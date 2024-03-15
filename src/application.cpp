#include "application.h"
#include "deck_connector_container.h"
#include "deck_formatter.h"
#include "deck_logger.h"
#include "deck_module.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <lua.hpp>
#include <memory_resource>
#include <string>
#include <string_view>
#include <thread>

struct ApplicationPrivate
{
	~ApplicationPrivate()
	{
		lua_close(state);
		delete mem_resource;
	}

	std::pmr::memory_resource* mem_resource;
	lua_State* state;
	DeckModule* deck_module;
	DeckLogger* deck_logger;
	std::string deckfile_file_name;
};

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
	DeckLogger* logger = reinterpret_cast<DeckLogger*>(lua_touserdata(L, lua_upvalueindex(1)));
	logger->push_this(L);
	lua_insert(L, 1);
	return logger->call(L);
};

} // namespace

#define L m_private->state

Application::Application()
    : m_private(new ApplicationPrivate)
{
	m_private->mem_resource = new std::pmr::unsynchronized_pool_resource({ 1024, 4096 }, std::pmr::new_delete_resource());

	L = lua_newstate(&_lua_alloc, m_private->mem_resource);
	lua_checkstack(L, 200);
	luaL_openlibs(L);

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	SDL_hid_init();
	IMG_Init(0xffffffff);
	TTF_Init();

	m_private->deck_module = DeckModule::create_new(L);
	lua_setglobal(L, "deck");

	m_private->deck_logger = DeckLogger::create_new(L);
	lua_setglobal(L, "logger");

	DeckFormatter::insert_enum_values(L);

	install_function_overrides();
}

Application::Application(Application&& other)
    : m_private(other.m_private)
{
	other.m_private = nullptr;
}

Application::~Application()
{
	TTF_Quit();
	IMG_Quit();
	SDL_hid_exit();
	SDL_Quit();

	delete m_private;
}

Application& Application::operator=(Application&& other)
{
	delete m_private;
	m_private       = other.m_private;
	other.m_private = nullptr;
	return *this;
}

bool Application::init(std::vector<std::string_view>&& args)
{
	int const oldtop = lua_gettop(L);

	std::string_view file_name;
	if (args.size() <= 1)
		file_name = "deckfile.lua";
	else
		file_name = args[1];
	m_private->deckfile_file_name = file_name;

	if (!LuaHelpers::load_script(L, m_private->deckfile_file_name.c_str()))
	{
		std::stringstream buf;
		LuaHelpers::print_error_context(buf);
		m_private->deck_logger->log_message(L, DeckLogger::Level::Error, buf.str());

		return false;
	}

	// TODO add safe mode by removing some dangerous stdlib functions

	// XXX remove me
	lua_pushvalue(L, -1);

	if (!LuaHelpers::pcall(L, 0, 0))
	{
		std::stringstream buf;
		LuaHelpers::print_error_context(buf);
		m_private->deck_logger->log_message(L, DeckLogger::Level::Error, buf.str());

		return false;
	}

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
	std::chrono::steady_clock::time_point clock = std::chrono::steady_clock::now();

	while (!m_private->deck_module->is_exit_requested())
	{
		auto const prev_clock = clock;
		clock                 = std::chrono::steady_clock::now();
		int const delta_msec  = std::chrono::duration_cast<std::chrono::milliseconds>(clock - prev_clock).count();

		int const resettop = lua_gettop(L);
		m_private->deck_module->tick(L, delta_msec);
		assert(lua_gettop(L) == resettop && "DeckModule tick function is not stack balanced");

		lua_getfield(L, LUA_REGISTRYINDEX, "ACTIVE_SCRIPT_ENV");
		lua_getfield(L, -1, "tick");
		if (lua_type(L, -1) == LUA_TFUNCTION)
		{
			lua_pushinteger(L, delta_msec);
			if (!LuaHelpers::pcall(L, 1, 0))
			{
				std::stringstream buf;
				LuaHelpers::print_error_context(buf);
				m_private->deck_logger->log_message(L, DeckLogger::Level::Error, buf.str());
				break;
			}
		}
		lua_settop(L, resettop);

		std::this_thread::sleep_until(clock + std::chrono::milliseconds(15));
	}

	m_private->deck_module->shutdown(L);

	return m_private->deck_module->get_exit_code();
}

void Application::install_function_overrides()
{
	lua_pushlightuserdata(L, m_private->deck_logger);
	lua_pushcclosure(L, &override_print, 1);
	lua_setglobal(L, "print");
}
