#ifndef DECK_ASSISTANT_DECK_TEXT_H
#define DECK_ASSISTANT_DECK_TEXT_H

#include "lua_class.h"
#include <string>

class DeckText : public LuaClass<DeckText>
{
public:
	static char const* LUA_TYPENAME;

	static void convert_top_of_stack(lua_State* L);

	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);
	int to_string(lua_State* L) const;

private:
	std::string m_text;
	std::string m_font;
};

#endif // DECK_ASSISTANT_DECK_TEXT_H
