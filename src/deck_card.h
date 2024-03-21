#ifndef DECK_ASSISTANT_DECK_CARD_H
#define DECK_ASSISTANT_DECK_CARD_H

#include "lua_class.h"
#include <SDL_surface.h>
#include <vector>

class DeckCard : public LuaClass<DeckCard>
{
public:
	DeckCard(SDL_Surface* surface);
	~DeckCard();

	inline SDL_Surface* get_surface() const { return m_surface; }

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);
	int tostring(lua_State* L) const;

	static SDL_Surface* resize_surface(SDL_Surface* surface, int new_width, int new_height);
	static std::vector<unsigned char> save_surface_as_bmp(SDL_Surface* surface);
	static std::vector<unsigned char> save_surface_as_jpeg(SDL_Surface* surface);
	static std::vector<unsigned char> save_surface_as_png(SDL_Surface* surface);

private:
	static int _lua_blit(lua_State* L);
	static int _lua_clear(lua_State* L);
	static int _lua_resize(lua_State* L);

private:
	SDL_Surface* m_surface;
};

#endif // DECK_ASSISTANT_DECK_CARD_H
