#ifndef DECK_ASSISTANT_DECK_TEXT_H
#define DECK_ASSISTANT_DECK_TEXT_H

#include "lua_class.h"
#include <SDL_surface.h>

class DeckText : public LuaClass<DeckText>
{
public:
	DeckText(SDL_Surface* surface);
	~DeckText();

	inline SDL_Surface* get_surface() const { return m_surface; }

	static char const* LUA_TYPENAME;
	int index(lua_State* L, std::string_view const& key) const;
	int tostring(lua_State* L) const;

private:
	SDL_Surface* m_surface;
};

#endif // DECK_ASSISTANT_DECK_TEXT_H
