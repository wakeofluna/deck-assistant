/*
 * DeckAssistant - Creating control panels using scripts.
 * Copyright (C) 2024  Esther Dalhuisen (Wake of Luna)
 *
 * DeckAssistant is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DeckAssistant is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef DECK_ASSISTANT_DECK_RECTANGLE_H
#define DECK_ASSISTANT_DECK_RECTANGLE_H

#include "lua_class.h"
#include <SDL_rect.h>

class DeckRectangle : public LuaClass<DeckRectangle>
{
public:
	DeckRectangle();
	DeckRectangle(SDL_Rect const& rect);
	DeckRectangle(int w, int h);
	DeckRectangle(int x, int y, int w, int h);

	inline bool contains(int x, int y) const { return contains(m_rectangle, x, y); }

	inline SDL_Rect const& get_rectangle() const { return m_rectangle; }
	inline SDL_Rect& get_rectangle() { return m_rectangle; }
	inline operator SDL_Rect*() const { return const_cast<SDL_Rect*>(&m_rectangle); }

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);
	int tostring(lua_State* L) const;

	static SDL_Rect centered(SDL_Rect const& object, SDL_Rect const& frame);
	static SDL_Rect clip(SDL_Rect const& lhs, SDL_Rect const& rhs);
	static bool contains(SDL_Rect const& rect, int x, int y);

private:
	static int _lua_centered(lua_State* L);
	static int _lua_contains(lua_State* L);
	static int _lua_set_size(lua_State* L);
	static int _lua_set_position(lua_State* L);
	static int _lua_move(lua_State* L);
	static int _lua_clip(lua_State* L);

private:
	SDL_Rect m_rectangle;
};

#endif // DECK_ASSISTANT_DECK_RECTANGLE_H
