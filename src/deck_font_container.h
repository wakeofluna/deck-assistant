#ifndef DECK_ASSISTANT_DECK_FONT_CONTAINER_H
#define DECK_ASSISTANT_DECK_FONT_CONTAINER_H

#include "lua_class.h"
#include <string>

class DeckFontContainer : public LuaClass<DeckFontContainer>
{
public:
	static char const* LUA_TYPENAME;

	static void push_global_instance(lua_State* L);
	static DeckFontContainer* get_global_instance(lua_State* L);

	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);

	int newindex(lua_State* L);

private:
	static int _lua_all(lua_State* L);
};

#endif // DECK_ASSISTANT_DECK_FONT_CONTAINER_H
