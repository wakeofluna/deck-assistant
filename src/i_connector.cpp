#include "i_connector.h"
#include "deck_connector.h"

IConnector::IConnector() = default;

IConnector::~IConnector() = default;

void IConnector::init_instance_table(lua_State* L)
{
}

IConnector* IConnector::from_stack(lua_State* L, int idx, char const* subtype_name, bool throw_error)
{
	DeckConnector* deck_connector = DeckConnector::from_stack(L, idx, throw_error);
	IConnector* connector         = deck_connector ? deck_connector->get_connector() : nullptr;

	if (connector && connector->get_subtype_name() == subtype_name)
		return connector;

	if (throw_error)
	{
		idx = LuaHelpers::absidx(L, idx);
		luaL_argerror(L, idx, "connector not of expected type");
	}

	return nullptr;
}

void IConnector::post_init(lua_State* L)
{
}

void IConnector::shutdown(lua_State* L)
{
}
