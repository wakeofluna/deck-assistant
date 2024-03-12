#ifndef DECK_ASSISTANT_DECK_CARD_H
#define DECK_ASSISTANT_DECK_CARD_H

#include "lua_class.h"

class DeckCard : public LuaClass<DeckCard>
{
public:
	static char const* LUA_TYPENAME;

	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
};

#endif // DECK_ASSISTANT_DECK_CARD_H
