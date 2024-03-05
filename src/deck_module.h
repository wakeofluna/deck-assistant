#ifndef DECK_ASSISTANT_DECK_MODULE_H
#define DECK_ASSISTANT_DECK_MODULE_H

#include "lua_class.h"
#include <string_view>

class DeckConnectorContainer;
class DeckFontContainer;

class DeckModule : public LuaClass<DeckModule>
{
public:
	static char const* LUA_TYPENAME;

	static char const* LUA_GLOBAL_INDEX_NAME;
	static void push_global_instance(lua_State* L);
	static DeckModule* get_global_instance(lua_State* L);

	inline DeckConnectorContainer* get_connector_container() const { return m_connector_container; }
	inline DeckFontContainer* get_font_container() const { return m_font_container; }

	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);

	int newindex(lua_State* L);

private:
	static int lua_create_impl(lua_State* L, lua_CFunction create_func);
	static int _lua_create_card(lua_State* L);
	static int _lua_create_component(lua_State* L);
	static int _lua_create_font(lua_State* L);

private:
	DeckConnectorContainer* m_connector_container;
	DeckFontContainer* m_font_container;
};

#endif // DECK_ASSISTANT_DECK_MODULE_H
