#ifndef DECK_ASSISTANT_DECK_MODULE_H
#define DECK_ASSISTANT_DECK_MODULE_H

#include "lua_class.h"
#include <chrono>
#include <optional>
#include <string_view>

class DeckConnectorContainer;

class DeckModule : public LuaClass<DeckModule>
{
public:
	DeckModule();

	static char const* LUA_TYPENAME;
	static char const* LUA_GLOBAL_INDEX_NAME;

	void tick(lua_State* L, int delta_msec);
	void shutdown(lua_State* L);

	bool is_exit_requested() const;
	int get_exit_code() const;

	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);

	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L);

private:
	static int _lua_create_card(lua_State* L);
	static int _lua_create_colour(lua_State* L);
	static int _lua_create_formatter(lua_State* L);
	static int _lua_create_image(lua_State* L);
	static int _lua_request_quit(lua_State* L);

private:
	lua_Integer m_total_delta;
	lua_Integer m_last_delta;
	DeckConnectorContainer* m_connector_container;
	std::optional<int> m_exit_requested;
};

#endif // DECK_ASSISTANT_DECK_MODULE_H
