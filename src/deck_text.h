#ifndef DECK_ASSISTANT_DECK_TEXT_H
#define DECK_ASSISTANT_DECK_TEXT_H

#include "lua_class.h"
#include <string>

class DeckText : public LuaClass<DeckText>
{
public:
	DeckText();
	DeckText(std::string_view const& value);

	static char const* LUA_TYPENAME;

	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);
	int tostring(lua_State* L) const;

private:
	std::string m_text;
	std::string m_font;
};

#endif // DECK_ASSISTANT_DECK_TEXT_H
