#ifndef DECK_ASSISTANT_DECK_RECTANGLE_LIST_H
#define DECK_ASSISTANT_DECK_RECTANGLE_LIST_H

#include "lua_class.h"
#include <vector>

class DeckRectangleList : public LuaClass<DeckRectangleList>
{
public:
	DeckRectangleList();
	~DeckRectangleList();

	static void push_any_contains(lua_State* L, int x, int y);

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, lua_Integer key);
	int newindex(lua_State* L, std::string_view const& key);
	int tostring(lua_State* L) const;

protected:
	static int _lua_add(lua_State* L);
	static int _lua_clear(lua_State* L);
	static int _lua_remove(lua_State* L);
	static int _lua_any_contains(lua_State* L);
	static int _lua_all_contains(lua_State* L);

protected:
	std::vector<int> m_refs;
};

#endif // DECK_ASSISTANT_DECK_RECTANGLE_LIST_H
