#include "deck_component.h"
#include "deck_text.h"
#include "lua_class.hpp"

template class LuaClass<DeckComponent>;

char const* DeckComponent::LUA_TYPENAME = "deck-assistant.DeckComponent";

void DeckComponent::init_class_table(lua_State* L)
{
}

void DeckComponent::init_instance_table(lua_State* L)
{
}

int DeckComponent::newindex(lua_State* L, lua_Integer key)
{
	lua_checkstack(L, lua_gettop(L) + 6);

	switch (lua_type(L, -1))
	{
		case LUA_TSTRING:
			{
				DeckComponent::push_new(L);
				lua_insert(L, -2);
				lua_setfield(L, -2, "text");
			}
			break;

		case LUA_TTABLE:
			{
				DeckComponent::push_new(L);
				lua_insert(L, -2);
				LuaHelpers::copy_table_fields(L);
			}
			break;

		default:
			break;
	}

	newindex_store_in_instance_table(L);
	return 0;
}

int DeckComponent::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "text")
		DeckText::convert_top_of_stack(L);

	newindex_store_in_instance_table(L);
	return 0;
}
