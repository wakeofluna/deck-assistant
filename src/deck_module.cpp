#include "deck_module.h"
#include "deck_card.h"
#include "deck_component.h"
#include "deck_connector.h"
#include "deck_connector_container.h"
#include "deck_font.h"
#include "deck_font_container.h"
#include "lua_class.hpp"

template class LuaClass<DeckModule>;

char const* DeckModule::LUA_TYPENAME          = "deck-assistant.DeckModule";
char const* DeckModule::LUA_GLOBAL_INDEX_NAME = "DeckModuleInstance";

DeckModule::DeckModule()
    : m_total_delta(0)
    , m_last_delta(0)
{
}

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

void DeckModule::tick(lua_State* L, int delta_msec)
{
	m_last_delta   = delta_msec;
	m_total_delta += delta_msec;
	m_connector_container->for_each(L, [delta_msec](lua_State* L, DeckConnector* dc) {
		dc->tick(L, delta_msec);
	});
}

void DeckModule::shutdown(lua_State* L)
{
	m_connector_container->for_each(L, [](lua_State* L, DeckConnector* dc) {
		dc->shutdown(L);
	});
}

bool DeckModule::is_exit_requested() const
{
	return m_exit_requested.has_value();
}

int DeckModule::get_exit_code() const
{
	return m_exit_requested.value_or(0);
}

void DeckModule::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &DeckModule::_lua_create_card);
	lua_setfield(L, -2, "Card");

	lua_pushcfunction(L, &DeckModule::_lua_create_component);
	lua_setfield(L, -2, "Component");

	lua_pushcfunction(L, &DeckModule::_lua_create_font);
	lua_setfield(L, -2, "Font");

	lua_pushcfunction(L, &DeckModule::_lua_request_quit);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "quit");
	lua_setfield(L, -2, "exit");
}

void DeckModule::init_instance_table(lua_State* L)
{
	m_font_container = DeckFontContainer::create_new(L);
	lua_setfield(L, -2, "fonts");

	m_connector_container = DeckConnectorContainer::create_new(L);
	lua_setfield(L, -2, "connectors");
}

int DeckModule::index(lua_State* L, std::string_view const& key) const
{
	if (key == "clock")
	{
		lua_pushinteger(L, m_total_delta);
	}
	else if (key == "delta")
	{
		lua_pushinteger(L, m_last_delta);
	}
	else
	{
		lua_pushnil(L);
	}
	return 1;
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

int DeckModule::_lua_request_quit(lua_State* L)
{
	DeckModule* self       = from_stack(L, 1);
	self->m_exit_requested = lua_tointeger(L, 2);
	return 0;
}
