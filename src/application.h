#ifndef DECK_ASSISTANT_APPLICATION_H
#define DECK_ASSISTANT_APPLICATION_H

#include <memory_resource>
#include <string_view>
#include <vector>

typedef struct lua_State lua_State;

class Application
{
public:
	Application();
	~Application();

	bool init(std::vector<std::string_view>&& args);
	int run();

private:
	static void* _lua_alloc(void* ud, void* ptr, size_t osize, size_t nsize);

private:
	std::pmr::memory_resource* m_mem_resource;
	lua_State* L;
};

#endif // DECK_ASSISTANT_APPLICATION_H
