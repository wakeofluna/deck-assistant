#include "application.h"
#include "deck_connector_container.h"
#include "deck_module.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <lua.hpp>
#include <memory_resource>
#include <string_view>
#include <thread>

namespace
{

constexpr const size_t largest_alloc_block = 4096;

constexpr size_t alloc_size_clamp(size_t value)
{
	if (value >= largest_alloc_block)
		return value;

	size_t target = 32;
	while (value > target)
		target <<= 1;
	return target;
}

struct ReaderData
{
	std::ifstream fp;
	std::vector<char> buffer;
};

} // namespace

Application::Application()
    : m_mem_resource(nullptr)
{
	m_mem_resource = new std::pmr::unsynchronized_pool_resource({ 1024, largest_alloc_block }, std::pmr::new_delete_resource());

	L = lua_newstate(&_lua_alloc, this);
	lua_checkstack(L, 200);

	luaL_openlibs(L);

	m_deck_module = DeckModule::create_new(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, DeckModule::LUA_GLOBAL_INDEX_NAME);
	lua_setglobal(L, "deck");
}

Application::~Application()
{
	lua_close(L);
	delete m_mem_resource;
}

bool Application::init(std::vector<std::string_view>&& args)
{
	if (args.size() <= 1)
		m_deckfile_file_name = "deckfile.lua";
	else
		m_deckfile_file_name = args[1];

	int oldtop = lua_gettop(L);

	int result = load_script(m_deckfile_file_name.c_str());
	if (result != LUA_OK)
	{
		switch (result)
		{
			case LUA_ERRFILE:
				std::cerr << "Unable to open/read file " << m_deckfile_file_name << std::endl;
				break;
			case LUA_ERRSYNTAX:
				std::cerr << "Syntax error in " << m_deckfile_file_name << std::endl;
				break;
			case LUA_ERRMEM:
				std::cerr << "Out of memory while loading " << m_deckfile_file_name << std::endl;
				break;
			default:
				std::cerr << "Lua error #" << result << " while loading " << m_deckfile_file_name << std::endl;
				break;
		}

		if (lua_gettop(L) > oldtop)
		{
			LuaHelpers::push_converted_to_string(L, -1);

			size_t err_msg_len;
			char const* err_msg = lua_tolstring(L, -1, &err_msg_len);
			if (err_msg)
				std::cerr << "Error: " << std::string_view(err_msg, err_msg_len) << std::endl;
		}

		lua_settop(L, oldtop);
		return false;
	}

	assert(lua_gettop(L) == oldtop + 1 && "Internal stack error while loading script");

	// TODO add safe mode by removing some dangerous stdlib functions

	// XXX remove me
	lua_pushvalue(L, -1);
	result = lua_pcall(L, 0, 0, 0);
	if (result != LUA_OK)
	{
		switch (result)
		{
			case LUA_ERRRUN:
				std::cerr << "Runtime error while running " << m_deckfile_file_name << std::endl;
				break;
			case LUA_ERRMEM:
				std::cerr << "Out of memory while running " << m_deckfile_file_name << std::endl;
				break;
			case LUA_ERRERR:
				std::cerr << "Internal error while running " << m_deckfile_file_name << std::endl;
				break;
			default:
				std::cerr << "Lua error #" << result << " while running " << m_deckfile_file_name << std::endl;
				break;
		}

		if (lua_gettop(L) > oldtop)
		{
			LuaHelpers::push_converted_to_string(L, -1);

			size_t err_msg_len;
			char const* err_msg = lua_tolstring(L, -1, &err_msg_len);
			if (err_msg)
				std::cerr << "Error: " << std::string_view(err_msg, err_msg_len) << std::endl;
		}

		lua_settop(L, oldtop);
		return false;
	}
	assert(lua_gettop(L) == oldtop + 1 && "Internal stack error while loading script");

	// Save a copy of the new global table
	lua_getfenv(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, "ACTIVE_SCRIPT_ENV");
	// Now pop the function, it has served its purpose
	lua_pop(L, 1);

	return true;
}

int Application::run()
{
	m_clock = std::chrono::steady_clock::now();

	while (true)
	{
		auto now   = std::chrono::steady_clock::now();
		auto delta = now - m_clock;
		m_clock    = now;

		int const resettop = lua_gettop(L);
		int delta_msec     = std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();
		m_deck_module->get_connector_container()->tick_all(L, delta_msec);
		assert(lua_gettop(L) >= resettop && "Connector tick functions are not stack balanced");
		lua_settop(L, resettop);

		lua_getfield(L, LUA_REGISTRYINDEX, "ACTIVE_SCRIPT_ENV");
		lua_getfield(L, -1, "tick");
		if (lua_type(L, -1) == LUA_TFUNCTION)
		{
			lua_pushinteger(L, delta_msec);
			int result = lua_pcall(L, 1, 0, 0);
			if (result != LUA_OK)
			{
				switch (result)
				{
					case LUA_ERRRUN:
						std::cerr << "Runtime error while running tick()" << std::endl;
						break;
					case LUA_ERRMEM:
						std::cerr << "Out of memory while running tick()" << std::endl;
						break;
					case LUA_ERRERR:
						std::cerr << "Internal error while running tick()" << std::endl;
						break;
					default:
						std::cerr << "Lua error #" << result << " while running tick()" << std::endl;
						break;
				}
			}
		}
		lua_settop(L, resettop);

		std::this_thread::sleep_until(m_clock + std::chrono::milliseconds(15));
	}

	return 0;
}

int Application::load_script(char const* file_name)
{
	ReaderData reader_data;

	reader_data.fp.open(file_name, std::ios::in | std::ios::binary);
	if (!reader_data.fp.good())
		return LUA_ERRFILE;

	reader_data.buffer.resize(4096);

	int result = lua_load(L, &Application::_lua_file_reader, &reader_data, file_name);
	if (result != LUA_OK)
		return result;

	// Create a new ENV table for the new chunk
	lua_createtable(L, 0, 0);
	// Create a metatable that indexes into the state global table
	lua_createtable(L, 0, 3);
	lua_pushboolean(L, true);
	lua_setfield(L, -2, "__metatable");
	lua_pushvalue(L, LUA_GLOBALSINDEX);
	lua_setfield(L, -2, "__index");
	lua_pushstring(L, file_name);
	lua_setfield(L, -2, "__name");
	//  Set the metatable then set the table as the ENV table
	lua_setmetatable(L, -2);
	lua_setfenv(L, -2);

	return LUA_OK;
}

void* Application::_lua_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
	Application* self = reinterpret_cast<Application*>(ud);

	if (ptr)
	{
		osize = alloc_size_clamp(osize);
		if (!nsize)
		{
			self->m_mem_resource->deallocate(ptr, osize);
			return nullptr;
		}
		else
		{
			nsize = alloc_size_clamp(nsize);
			if (osize == nsize)
				return ptr;

			void* new_mem = self->m_mem_resource->allocate(nsize, sizeof(void*));
			if (osize > nsize)
			{
				std::memcpy(new_mem, ptr, nsize);
			}
			else
			{
				std::memcpy(new_mem, ptr, osize);
				if (nsize > osize)
					std::memset((char*)new_mem + osize, 0, nsize - osize);
			}

			self->m_mem_resource->deallocate(ptr, osize);
			return new_mem;
		}
	}
	else
	{
		nsize = alloc_size_clamp(nsize);

		void* new_mem = self->m_mem_resource->allocate(nsize, sizeof(void*));
		std::memset(new_mem, 0, nsize);
		return new_mem;
	}
}

char const* Application::_lua_file_reader(lua_State* L, void* data, size_t* size)
{
	ReaderData* reader_data = reinterpret_cast<ReaderData*>(data);
	if (!reader_data->fp.good())
		return nullptr;

	reader_data->fp.read(reader_data->buffer.data(), reader_data->buffer.size());
	*size = reader_data->fp.gcount();
	return reader_data->buffer.data();
}
