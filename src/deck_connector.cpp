#include "deck_connector.h"
#include "lua_class.hpp"

template class LuaClass<DeckConnector>;

namespace
{

template <typename... ARGS>
void run_connector_function(lua_State* L, DeckConnector* self, void (IConnector::*func)(lua_State*, ARGS...), ARGS... args)
{
	lua_checkstack(L, lua_gettop(L) + 20);

	self->push_this(L);
	LuaHelpers::push_instance_table(L, -1);

	int oldtop = lua_gettop(L);
	(self->get_connector()->*func)(L, std::forward<ARGS>(args)...);
	assert(lua_gettop(L) >= oldtop && "DeckConnector internal function was not stack balanced");

	lua_settop(L, oldtop - 2);
}

} // namespace

char const* DeckConnector::LUA_TYPENAME = "deck-assistant.DeckConnector";

DeckConnector::DeckConnector(std::unique_ptr<IConnector>&& connector)
    : m_connector(std::move(connector))
{
	assert(m_connector && "DeckConnector must have a valid IConnector instance");
}

DeckConnector::~DeckConnector() = default;

void DeckConnector::init_instance_table(lua_State* L)
{
	m_connector->init_instance_table(L);
}

void DeckConnector::post_init(lua_State* L)
{
	run_connector_function(L, this, &IConnector::post_init);
}

void DeckConnector::tick(lua_State* L, int delta_msec)
{
	run_connector_function(L, this, &IConnector::tick, delta_msec);
}

int DeckConnector::index(lua_State* L) const
{
	DeckConnector const* self = from_stack(L, 1);
	push_instance_table(L, 1);
	lua_replace(L, 1);
	return self->m_connector->index(L);
}

int DeckConnector::newindex(lua_State* L)
{
	DeckConnector* self = from_stack(L, 1);
	push_instance_table(L, 1);
	lua_replace(L, 1);
	return self->m_connector->newindex(L);
}

int DeckConnector::to_string(lua_State* L) const
{
	lua_pushfstring(L, "%s: %p: %s", type_name(), this, m_connector->get_subtype_name());
	return 1;
}
