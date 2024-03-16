#include "deck_module.h"
#include "deck_card.h"
#include "deck_colour.h"
#include "deck_connector.h"
#include "deck_connector_container.h"
#include "deck_formatter.h"
#include "deck_image.h"
#include "lua_class.hpp"

template class LuaClass<DeckModule>;

char const* DeckModule::LUA_TYPENAME = "deck:DeckModule";

DeckModule::DeckModule()
    : m_last_clock(0)
    , m_last_delta(0)
{
}

void DeckModule::tick(lua_State* L, lua_Integer clock)
{
	m_last_delta = clock - m_last_clock;
	m_last_clock = clock;

	DeckConnectorContainer* container = get_connector_container(L);
	container->for_each(L, [this](lua_State* L, DeckConnector* dc) {
		dc->tick(L, m_last_delta);
	});

	lua_pop(L, 1);
}

void DeckModule::shutdown(lua_State* L)
{
	DeckConnectorContainer* container = get_connector_container(L);
	container->for_each(L, [](lua_State* L, DeckConnector* dc) {
		dc->shutdown(L);
	});

	lua_pop(L, 1);
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

	lua_pushcfunction(L, &DeckModule::_lua_create_colour);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "Colour");
	lua_setfield(L, -2, "Color");

	lua_pushcfunction(L, &DeckModule::_lua_create_formatter);
	lua_setfield(L, -2, "Formatter");

	lua_pushcfunction(L, &DeckModule::_lua_create_image);
	lua_setfield(L, -2, "Image");

	lua_pushcfunction(L, &DeckModule::_lua_request_quit);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "quit");
	lua_setfield(L, -2, "exit");
}

void DeckModule::init_instance_table(lua_State* L)
{
	DeckConnectorContainer::create_new(L);
	lua_setfield(L, -2, "connectors");
}

int DeckModule::index(lua_State* L, std::string_view const& key) const
{
	if (key == "clock")
	{
		lua_pushinteger(L, m_last_clock);
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

DeckConnectorContainer* DeckModule::get_connector_container(lua_State* L)
{
	assert(from_stack(L, -1, false) && "DeckModule::get_connector_container needs self on -1");
	lua_getfield(L, -1, "connectors");
	DeckConnectorContainer* container = DeckConnectorContainer::from_stack(L, -1, false);
	assert(container && "DeckModule lost its connector container");
	return container;
}

int DeckModule::_lua_create_card(lua_State* L)
{
	from_stack(L, 1);
	int width  = check_arg_int(L, 2);
	int height = check_arg_int(L, 3);
	DeckCard::create_new(L, width, height);
	return 1;
}

int DeckModule::_lua_create_colour(lua_State* L)
{
	from_stack(L, 1);
	lua_settop(L, 2);
	DeckColour::convert_top_of_stack(L);
	return 1;
}

int DeckModule::_lua_create_formatter(lua_State* L)
{
	from_stack(L, 1);
	lua_settop(L, 2);
	DeckFormatter::convert_top_of_stack(L);
	return 1;
}

int DeckModule::_lua_create_image(lua_State* L)
{
	from_stack(L, 1);
	lua_settop(L, 2);
	DeckImage::convert_top_of_stack(L);
	return 1;
}

int DeckModule::_lua_request_quit(lua_State* L)
{
	DeckModule* self       = from_stack(L, 1);
	self->m_exit_requested = lua_tointeger(L, 2);
	return 0;
}
