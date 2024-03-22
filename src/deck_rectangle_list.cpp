#include "deck_rectangle_list.h"
#include "deck_rectangle.h"
#include "lua_helpers.h"
#include <cassert>

char const* DeckRectangleList::LUA_TYPENAME = "deck:RectangleList";

DeckRectangleList::DeckRectangleList()  = default;
DeckRectangleList::~DeckRectangleList() = default;

void DeckRectangleList::push_any_contains(lua_State* L, int x, int y)
{
	DeckRectangleList* self = from_stack(L, -1, false);
	assert(self != nullptr && "DeckRectangleList::push_any_contains requires self on top of stack");

	LuaHelpers::push_instance_table(L, -1);

	auto iter = self->m_refs.cbegin();
	auto end  = self->m_refs.cend();
	for (; iter != end; ++iter)
	{
		lua_rawgeti(L, -1, *iter);
		DeckRectangle* rect = DeckRectangle::from_stack(L, -1);
		if (rect->contains(x, y))
		{
			lua_replace(L, -2);
			return;
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	lua_pushnil(L);
}

void DeckRectangleList::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &_lua_add);
	lua_setfield(L, -2, "add");

	lua_pushcfunction(L, &_lua_clear);
	lua_setfield(L, -2, "clear");

	lua_pushcfunction(L, &_lua_remove);
	lua_setfield(L, -2, "remove");

	lua_pushcfunction(L, &_lua_any_contains);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "contains");
	lua_setfield(L, -2, "any");

	lua_pushcfunction(L, &_lua_all_contains);
	lua_setfield(L, -2, "all");
}

int DeckRectangleList::index(lua_State* L, std::string_view const& key) const
{
	if (key == "size" || key == "len" || key == "count")
	{
		lua_pushinteger(L, m_refs.size());
	}
	else
	{
		lua_pushnil(L);
	}
	return 1;
}

int DeckRectangleList::newindex(lua_State* L, lua_Integer key)
{
	luaL_error(L, "integer slots are reserved for internal use for %s", LUA_TYPENAME);
	return 0;
}

int DeckRectangleList::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "size" || key == "len" || key == "count")
	{
		luaL_error(L, "key %s is readonly for %s", key.data(), LUA_TYPENAME);
	}
	else
	{
		LuaHelpers::newindex_store_in_instance_table(L);
	}
	return 0;
}

int DeckRectangleList::tostring(lua_State* L) const
{
	lua_pushfstring(L, "%s { %d items }", LUA_TYPENAME, int(m_refs.size()));
	return 1;
}

int DeckRectangleList::_lua_add(lua_State* L)
{
	DeckRectangleList* self = from_stack(L, 1);
	int const max_idx       = lua_gettop(L);

	LuaHelpers::push_instance_table(L, 1);

	for (int idx = 2; idx <= max_idx; ++idx)
	{
		DeckRectangle::from_stack(L, idx);
		lua_pushvalue(L, idx);
		int ref = luaL_ref(L, -2);
		self->m_refs.push_back(ref);
	}

	return 0;
}

int DeckRectangleList::_lua_clear(lua_State* L)
{
	DeckRectangleList* self = from_stack(L, 1);

	lua_settop(L, 1);
	LuaHelpers::push_instance_table(L, 1);

	for (int const ref : self->m_refs)
		luaL_unref(L, -1, ref);

	lua_pushinteger(L, self->m_refs.size());
	self->m_refs.clear();
	return 1;
}

int DeckRectangleList::_lua_remove(lua_State* L)
{
	DeckRectangleList* self = from_stack(L, 1);
	int const max_idx       = lua_gettop(L);

	LuaHelpers::push_instance_table(L, 1);
	int removed = 0;

	for (int idx = 2; idx <= max_idx; ++idx)
	{
		DeckRectangle::from_stack(L, idx);
		auto iter = self->m_refs.begin();
		auto end  = self->m_refs.end();
		for (; iter != end; ++iter)
		{
			lua_rawgeti(L, -1, *iter);
			bool equal = lua_rawequal(L, -1, idx);
			lua_pop(L, 1);

			if (equal)
			{
				luaL_unref(L, -1, *iter);
				self->m_refs.erase(iter);
				++removed;
				break;
			}
		}
	}

	lua_pushinteger(L, removed);
	return 1;
}

int DeckRectangleList::_lua_any_contains(lua_State* L)
{
	from_stack(L, 1);
	int x = LuaHelpers::check_arg_int(L, 2);
	int y = LuaHelpers::check_arg_int(L, 3);

	lua_settop(L, 1);
	push_any_contains(L, x, y);
	return 1;
}

int DeckRectangleList::_lua_all_contains(lua_State* L)
{
	DeckRectangleList* self = from_stack(L, 1);
	int x                   = LuaHelpers::check_arg_int(L, 2);
	int y                   = LuaHelpers::check_arg_int(L, 3);

	lua_settop(L, 1);
	lua_createtable(L, 4, 0);
	LuaHelpers::push_instance_table(L, 1);
	std::size_t nr_found = 0;

	auto iter = self->m_refs.cbegin();
	auto end  = self->m_refs.cend();
	for (; iter != end; ++iter)
	{
		lua_rawgeti(L, -1, *iter);
		DeckRectangle* rect = DeckRectangle::from_stack(L, -1);
		if (rect->contains(x, y))
		{
			++nr_found;
			lua_rawseti(L, -3, nr_found);
		}
		else
		{
			lua_pop(L, 1);
		}
	}

	lua_pop(L, 1);
	assert(lua_type(L, -1) == LUA_TTABLE);
	assert(lua_objlen(L, -1) == nr_found);
	return 1;
}
