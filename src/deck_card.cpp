#include "deck_card.h"
#include "lua_class.hpp"

template class LuaClass<DeckCard>;

char const* DeckCard::LUA_TYPENAME = "deck-assistant.DeckCard";

void DeckCard::init_class_table(lua_State* L)
{
}

void DeckCard::init_instance_table(lua_State* L)
{
}
