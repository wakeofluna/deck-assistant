#include "application.h"
#include "deck_font.h"
#include "deck_logger.h"
#include "deck_module.h"
#include <SDL.h>
#include <SDL_image.h>
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
};

} // namespace

Application::Application()
{
	m_mem_resource = new std::pmr::unsynchronized_pool_resource({ 1024, 4096 }, std::pmr::new_delete_resource());

	L = lua_newstate(&_lua_alloc, m_mem_resource);
	lua_checkstack(L, 200);
	luaL_openlibs(L);

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	SDL_hid_init();
	IMG_Init(0xffffffff);
	TTF_Init();

	DeckModule::create_new(L);
	lua_setglobal(L, "deck");

	DeckLogger::create_new(L);
	lua_setglobal(L, "logger");

	DeckFont::insert_enum_values(L);

	install_function_overrides();
}

Application::~Application()
{
	TTF_Quit();
	IMG_Quit();
	SDL_hid_exit();
	SDL_Quit();

	lua_close(L);
	delete m_mem_resource;
}

bool Application::init(std::vector<std::string_view>&& args)
{
	int const oldtop = lua_gettop(L);

	std::string_view file_name;
	if (args.size() <= 1)
		file_name = "deckfile.lua";
	else
		file_name = args[1];
	m_deckfile_file_name = file_name;

	if (!LuaHelpers::load_script(L, m_deckfile_file_name.c_str()))
		return false;

	// TODO add safe mode by removing some dangerous stdlib functions

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

	DeckModule* deck_module = DeckModule::push_global_instance(L);
	int const resettop      = lua_gettop(L);

	while (!deck_module->is_exit_requested())
	{
		auto const clock             = std::chrono::steady_clock::now();
		lua_Integer const clock_msec = std::chrono::duration_cast<std::chrono::milliseconds>(clock - start_time).count();

		deck_module->tick(L, clock_msec);
		assert(lua_gettop(L) == resettop && "DeckModule tick function is not stack balanced");

		lua_getfield(L, LUA_REGISTRYINDEX, "ACTIVE_SCRIPT_ENV");
		lua_getfield(L, -1, "tick");
		if (lua_type(L, -1) == LUA_TFUNCTION)
		{
			lua_pushinteger(L, clock_msec);
			LuaHelpers::pcall(L, 1, 0);
		}
		lua_pop(L, 1);

		std::this_thread::sleep_until(clock + std::chrono::milliseconds(15));
	}

	assert(lua_gettop(L) == resettop && "DeckModule run loop is not stack balanced");

	deck_module->shutdown(L);
	int exit_code = deck_module->get_exit_code();

	lua_pop(L, 1);
	return exit_code;
}

void Application::install_function_overrides()
{
	DeckLogger::push_global_instance(L);
	lua_pushcclosure(L, &override_print, 1);
	lua_setglobal(L, "print");
}
