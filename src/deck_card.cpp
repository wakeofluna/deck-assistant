#include "deck_card.h"
#include "deck_component.h"
#include "lua_class.hpp"

template class LuaClass<DeckCard>;

char const* DeckCard::LUA_TYPENAME = "deck-assistant.DeckCard";

void DeckCard::init_class_table(lua_State* L)
{
}

void DeckCard::init_instance_table(lua_State* L)
{
}

int DeckCard::newindex(lua_State* L, lua_Integer key)
{
	if (key == 1 && lua_type(L, 3) == LUA_TTABLE)
		DeckComponent::convert_top_of_stack(L);

	if (key == 1 && DeckComponent::is_on_stack(L, -1))
	{
		push_instance_table(L, 1);
		lua_getfield(L, -1, "root");
		if (lua_type(L, -1) == LUA_TNIL)
		{
			lua_pushvalue(L, -3);
			lua_setfield(L, -3, "root");
		}
		lua_pop(L, 2);
	}

	newindex_store_in_instance_table(L);
	return 0;
}

int DeckCard::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "root")
		DeckComponent::convert_top_of_stack(L);

	newindex_store_in_instance_table(L);
	return 0;
}
