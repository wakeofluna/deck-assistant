#ifndef DECK_ASSISTANT_DECK_MODULE_H
#define DECK_ASSISTANT_DECK_MODULE_H

#include "lua_class.h"
#include <string_view>

class DeckModule : public LuaClass<DeckModule>
{
public:
	DeckModule();
	~DeckModule();

	static char const* LUA_TYPENAME;

	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
};

#endif // DECK_ASSISTANT_DECK_MODULE_H
