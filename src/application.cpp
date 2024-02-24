#include "application.h"
#include <cstring>
#include <lua.hpp>
#include <memory_resource>

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

} // namespace

Application::Application()
    : m_mem_resource(nullptr)
{
	m_mem_resource = new std::pmr::unsynchronized_pool_resource({ 1024, largest_alloc_block }, std::pmr::new_delete_resource());

	L = lua_newstate(&_lua_alloc, this);
	luaL_openlibs(L);
}

Application::~Application()
{
	lua_close(L);
	delete m_mem_resource;
}

bool Application::init(std::vector<std::string_view>&& args)
{
	return true;
}

int Application::run()
{
	return 0;
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
