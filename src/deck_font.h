#ifndef DECK_ASSISTANT_DECK_FONT_H
#define DECK_ASSISTANT_DECK_FONT_H

#include "lua_class.h"
#include <string>

class DeckFont : public LuaClass<DeckFont>
{
public:
	DeckFont();
	DeckFont(std::string_view const& value);

	static char const* LUA_TYPENAME;

	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);
	int tostring(lua_State* L) const;

private:
	std::string m_font_name;
	std::size_t m_font_size;
};

#endif // DECK_ASSISTANT_DECK_FONT_H
