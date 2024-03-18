#ifndef DECK_ASSISTANT_DECK_RECTANGLE_H
#define DECK_ASSISTANT_DECK_RECTANGLE_H

#include "lua_class.h"
#include <SDL_rect.h>

class DeckRectangle : public LuaClass<DeckRectangle>
{
public:
	DeckRectangle();
	DeckRectangle(int w, int h);
	DeckRectangle(int x, int y, int w, int h);

	inline SDL_Rect const& get_rectangle() const { return m_rectangle; }
	inline SDL_Rect& get_rectangle() { return m_rectangle; }
	inline operator SDL_Rect*() const { return const_cast<SDL_Rect*>(&m_rectangle); }

	static char const* LUA_TYPENAME;

	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);
	int tostring(lua_State* L) const;

private:
	SDL_Rect m_rectangle;
};

#endif // DECK_ASSISTANT_DECK_RECTANGLE_H
