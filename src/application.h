#ifndef DECK_ASSISTANT_APPLICATION_H
#define DECK_ASSISTANT_APPLICATION_H

#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

typedef struct lua_State lua_State;

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

private:
	void install_function_overrides();

private:
	lua_State* L;
	std::pmr::memory_resource* m_mem_resource;
	std::string m_deckfile_file_name;
};

#endif // DECK_ASSISTANT_APPLICATION_H
