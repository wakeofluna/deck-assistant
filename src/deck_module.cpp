#include "deck_module.h"
#include "deck_card.h"
#include "deck_component.h"
#include "deck_font.h"
#include "deck_font_container.h"
#include "lua_class.hpp"

template class LuaClass<DeckModule>;

char const* DeckModule::LUA_TYPENAME          = "deck-assistant.DeckModule";
char const* DeckModule::LUA_GLOBAL_INDEX_NAME = "DeckModuleInstance";

void DeckModule::push_global_instance(lua_State* L)
{
	lua_getfield(L, LUA_REGISTRYINDEX, LUA_GLOBAL_INDEX_NAME);
}

DeckModule* DeckModule::get_global_instance(lua_State* L)
{
	push_global_instance(L);
	DeckModule* instance = DeckModule::from_stack(L, -1, true);
	lua_pop(L, 1);
	return instance;
}

void DeckModule::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &DeckModule::_lua_create_card);
	lua_setfield(L, -2, "Card");

	lua_pushcfunction(L, &DeckModule::_lua_create_component);
	lua_setfield(L, -2, "Component");

	lua_pushcfunction(L, &DeckModule::_lua_create_font);
	lua_setfield(L, -2, "Font");
}

void DeckModule::init_instance_table(lua_State* L)
{
	DeckFontContainer::push_new(L);
	lua_setfield(L, -2, "fonts");
}

int DeckModule::newindex(lua_State* L)
{
	luaL_error(L, "%s instance is closed for modifications", type_name());
	return 0;
}

int DeckModule::lua_create_impl(lua_State* L, lua_CFunction create_func)
{
	from_stack(L, 1, true);

	assert(create_func);
	(*create_func)(L);

	switch (lua_type(L, 2))
	{
		case LUA_TNONE:
			break;
		case LUA_TTABLE:
			lua_pushvalue(L, 2);
			copy_table_fields(L);
			break;
		default:
			luaL_typerror(L, 2, "table or none");
			break;
	}

	return 1;
}

int DeckModule::_lua_create_card(lua_State* L)
{
	return lua_create_impl(L, &DeckCard::push_new);
}

int DeckModule::_lua_create_component(lua_State* L)
{
	return lua_create_impl(L, &DeckComponent::push_new);
}

int DeckModule::_lua_create_font(lua_State* L)
{
	return lua_create_impl(L, &DeckFont::push_new);
}
