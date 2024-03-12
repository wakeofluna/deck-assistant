#ifndef DECK_ASSISTANT_DECK_IMAGE_H
#define DECK_ASSISTANT_DECK_IMAGE_H

#include "lua_class.h"
#include <string>

class DeckImage : public LuaClass<DeckImage>
{
public:
	DeckImage();
	DeckImage(std::string_view const& value);

	static char const* LUA_TYPENAME;

	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);
	int tostring(lua_State* L) const;

private:
	std::string m_src;
	int m_src_width;
	int m_src_height;
	int m_wanted_width;
	int m_wanted_height;
};

#endif // DECK_ASSISTANT_DECK_IMAGE_H
