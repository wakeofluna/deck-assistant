#ifndef DECK_ASSISTANT_DECK_COLOUR_H
#define DECK_ASSISTANT_DECK_COLOUR_H

#include "lua_class.h"
#include "util_colour.h"

class DeckColour : public LuaClass<DeckColour>
{
public:
	DeckColour();
	DeckColour(std::string_view const& value);
	DeckColour(Colour c);

	inline Colour get_colour() const { return m_colour; }

	static char const* LUA_TYPENAME;

	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);
	int tostring(lua_State* L) const;

private:
	Colour m_colour;
};

#endif // DECK_ASSISTANT_DECK_COLOUR_H
