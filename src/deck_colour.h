#ifndef DECK_ASSISTANT_DECK_COLOUR_H
#define DECK_ASSISTANT_DECK_COLOUR_H

#include "lua_class.h"
#include <cstdint>

struct Colour
{
	constexpr inline void clear() { value = 0; }
	constexpr inline void set_pink()
	{
		r = 0xEE;
		g = 0x82;
		b = 0xEE;
		a = 0xFF;
	}

	union
	{
		struct
		{
			unsigned char r;
			unsigned char g;
			unsigned char b;
			unsigned char a;
		};
		std::uint32_t value;
	};
};

class DeckColour : public LuaClass<DeckColour>
{
public:
	DeckColour();
	DeckColour(std::string_view const& value);

	static char const* LUA_TYPENAME;

	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);
	int tostring(lua_State* L) const;

private:
	Colour m_colour;
};

#endif // DECK_ASSISTANT_DECK_COLOUR_H
