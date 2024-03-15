#ifndef DECK_ASSISTANT_DECK_CARD_H
#define DECK_ASSISTANT_DECK_CARD_H

#include "lua_class.h"
#include <SDL_surface.h>

class DeckCard : public LuaClass<DeckCard>
{
public:
	DeckCard(int width, int height);
	~DeckCard();

	inline SDL_Surface* get_surface() const { return m_surface; }

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int tostring(lua_State* L) const;

private:
	static int _lua_blit(lua_State* L);
	static int _lua_clear(lua_State* L);

private:
	SDL_Surface* m_surface;
};

#endif // DECK_ASSISTANT_DECK_CARD_H
