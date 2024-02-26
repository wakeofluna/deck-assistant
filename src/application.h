#ifndef DECK_ASSISTANT_APPLICATION_H
#define DECK_ASSISTANT_APPLICATION_H

#include <string>
#include <string_view>
#include <vector>

typedef struct lua_State lua_State;

namespace std::pmr
{
class memory_resource;
}

class DeckModule;

class Application
{
public:
	Application();
	~Application();

	bool init(std::vector<std::string_view>&& args);
	int run();

protected:
	int load_script(char const* file_name);

private:
	static void* _lua_alloc(void* ud, void* ptr, size_t osize, size_t nsize);
	static char const* _lua_file_reader(lua_State* L, void* data, size_t* size);

private:
	std::pmr::memory_resource* m_mem_resource;
	lua_State* L;
	DeckModule* m_deck_module;
	std::string m_deckfile_file_name;
};

#endif // DECK_ASSISTANT_APPLICATION_H
