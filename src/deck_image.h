#ifndef DECK_ASSISTANT_DECK_IMAGE_H
#define DECK_ASSISTANT_DECK_IMAGE_H

#include "deck_colour.h"
#include "lua_class.h"
#include <SDL.h>
#include <string>
#include <vector>

class DeckImage : public LuaClass<DeckImage>
{
public:
	DeckImage();
	~DeckImage();

	static char const* LUA_TYPENAME;

	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);
	int tostring(lua_State* L) const;

	SDL_Surface* get_surface() const;

	static SDL_Surface* resize_surface(SDL_Surface* surface, int new_width, int new_height);
	static std::vector<unsigned char> save_surface_as_png(SDL_Surface* surface);
	static std::vector<unsigned char> save_surface_as_jpeg(SDL_Surface* surface);

private:
	bool load_surface(lua_State* L);
	void free_surface();

private:
	std::string m_src;
	SDL_Surface* m_surface;
};

#endif // DECK_ASSISTANT_DECK_IMAGE_H
