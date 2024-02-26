#include "deck_module.h"
#include "lua_class.hpp"

template class LuaClass<DeckModule>;

char const* DeckModule::LUA_TYPENAME = "deck-assistant.DeckModule";

DeckModule::DeckModule()
{
}

DeckModule::~DeckModule()
{
}

void DeckModule::init_class_table(lua_State* L)
{
}

void DeckModule::init_instance_table(lua_State* L)
{
}
