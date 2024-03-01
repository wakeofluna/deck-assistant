#ifndef DECK_ASSISTANT_DECK_FONT_H
#define DECK_ASSISTANT_DECK_FONT_H

#include "lua_class.h"
#include <string>

class DeckFont : public LuaClass<DeckFont>
{
public:
	DeckFont();
	~DeckFont();

	static char const* LUA_TYPENAME;

	static void init_class_table(lua_State* L);

	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);
	int to_string(lua_State* L) const;

private:
	static int _lua_clone(lua_State* L);

private:
	std::string m_font_name;
	std::size_t m_font_size;
};

#endif // DECK_ASSISTANT_DECK_FONT_H
