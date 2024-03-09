#include "deck_font_container.h"
#include "deck_font.h"
#include "deck_module.h"
#include "lua_class.hpp"

template class LuaClass<DeckFontContainer>;

char const* DeckFontContainer::LUA_TYPENAME = "deck-assistant.DeckFontContainer";

void DeckFontContainer::push_global_instance(lua_State* L)
{
	DeckModule::push_global_instance(L);
	lua_getfield(L, -1, "fonts");
	lua_replace(L, -2);
}

DeckFontContainer* DeckFontContainer::get_global_instance(lua_State* L)
{
	DeckModule::push_global_instance(L);
	lua_getfield(L, -1, "fonts");
	DeckFontContainer* container = DeckFontContainer::from_stack(L, -1, true);
	lua_pop(L, 2);
	return container;
}

void DeckFontContainer::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &DeckFontContainer::_lua_all);
	lua_setfield(L, -2, "all");
}

void DeckFontContainer::init_instance_table(lua_State* L)
{
	DeckFont::create_new(L);
	lua_setfield(L, -2, "default");
}

int DeckFontContainer::newindex(lua_State* L)
{
	from_stack(L, 1);
	check_arg_string(L, 2);
	DeckFont::convert_top_of_stack(L);
	newindex_store_in_instance_table(L);
	return 0;
}

int DeckFontContainer::_lua_all(lua_State* L)
{
	from_stack(L, 1);
	push_instance_table(L, 1);
	return 1;
}
