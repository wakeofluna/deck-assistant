#include "deck_connector_container.h"
#include "connector_elgato_streamdeck.h"
#include "deck_connector.h"
#include <cassert>

namespace
{

bool create_connector_arg_check(lua_State* L)
{
	DeckConnectorContainer::from_stack(L, 1, true);
	LuaHelpers::check_arg_string(L, 2, false);

	int const vtype3 = lua_type(L, 3);
	if (vtype3 != LUA_TTABLE && vtype3 != LUA_TNONE)
		luaL_typerror(L, 3, "table or none");

	lua_settop(L, vtype3 == LUA_TTABLE ? 3 : 2);
	return vtype3 == LUA_TTABLE;
}

int connector_not_available(lua_State* L, char const* message)
{
	create_connector_arg_check(L);
	luaL_error(L, message);
	return 0;
}

template <typename T>
int get_existing_or_create_new(lua_State* L)
{
	bool do_table_init = create_connector_arg_check(L);

	LuaHelpers::push_instance_table(L, 1);
	lua_pushvalue(L, 2);
	lua_pushvalue(L, 2);
	lua_rawget(L, -3);

	DeckConnector* connector = nullptr;

	if (lua_type(L, -1) == LUA_TUSERDATA)
	{
		DeckConnector* old_connector = DeckConnector::from_stack(L, -1, false);
		IConnector const* existing   = old_connector ? old_connector->get_connector() : nullptr;
		if (existing)
		{
			std::string_view type1 = existing->get_subtype_name();
			std::string_view type2 = T::SUBTYPE_NAME;
			if (type1 == type2)
				connector = old_connector;
		}
	}

	if (!connector)
	{
		lua_pop(L, 1);
		std::unique_ptr<IConnector> new_connector = std::make_unique<T>();
		connector                                 = DeckConnector::push_new(L, std::move(new_connector));
	}

	if (do_table_init)
	{
		lua_pushvalue(L, 3);
		LuaHelpers::copy_table_fields(L);
	}

	lua_pushvalue(L, -1);
	lua_insert(L, -4);
	lua_rawset(L, -3);
	lua_pop(L, 1);

	connector->post_init(L);

	return 1;
}

} // namespace

char const* DeckConnectorContainer::LUA_TYPENAME = "deck:DeckConnectorContainer";

void DeckConnectorContainer::for_each(lua_State* L, std::function<void(lua_State* L, DeckConnector*)> const& visitor) const
{
	int const resettop = lua_gettop(L);
	lua_checkstack(L, resettop + 24);

	assert(DeckConnectorContainer::from_stack(L, -1, false) && "DeckConnectorContainer::for_each needs self on -1");
	push_instance_table(L, -1);

	lua_pushnil(L);
	while (lua_next(L, -2) != 0)
	{
		int const checktop = lua_gettop(L);

		DeckConnector* connector = DeckConnector::from_stack(L, -1, false);
		if (connector)
			visitor(L, connector);

		assert(lua_gettop(L) >= checktop && "DeckConnector visitor function was not stack balanced");
		lua_settop(L, checktop - 1);
	}

	lua_settop(L, resettop);
}

void DeckConnectorContainer::init_class_table(lua_State* L)
{
	// Prevent possible unused warnings
	(void)connector_not_available;

	lua_pushcfunction(L, &DeckConnectorContainer::_lua_all);
	lua_setfield(L, -2, "all");

	lua_pushcfunction(L, &DeckConnectorContainer::_lua_create_elgato_streamdeck);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "ElgatoStreamDeck");
	lua_setfield(L, -2, "StreamDeck");
}

void DeckConnectorContainer::init_instance_table(lua_State* L)
{
	LuaHelpers::push_standard_weak_value_metatable(L);
	lua_setmetatable(L, -2);
}

int DeckConnectorContainer::newindex(lua_State* L)
{
	luaL_error(L, "%s instance is closed for modifications", type_name());
	return 0;
}

int DeckConnectorContainer::_lua_all(lua_State* L)
{
	from_stack(L, 1);
	push_instance_table(L, 1);
	return 1;
}

int DeckConnectorContainer::_lua_create_elgato_streamdeck(lua_State* L)
{
	return get_existing_or_create_new<ConnectorElgatoStreamDeck>(L);
}
