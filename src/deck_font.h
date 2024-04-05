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

#ifndef DECK_ASSISTANT_DECK_FONT_H
#define DECK_ASSISTANT_DECK_FONT_H

#include "lua_class.h"
#include "util_colour.h"
#include <SDL_ttf.h>
#include <string>

class DeckFont : public LuaClass<DeckFont>
{
public:
	enum class Alignment : char
	{
		Left,
		Center,
		Right,
	};

	enum class Style : char
	{
		Regular,
		Bold,
		Italic,
		Underline,
		Strikethrough,
	};

public:
	DeckFont();
	DeckFont(DeckFont const&);

	static void insert_enum_values(lua_State* L);

	static char const* LUA_TYPENAME;

	static void init_class_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, lua_Integer key);
	int newindex(lua_State* L, std::string_view const& key);
	int tostring(lua_State* L) const;

private:
	void load_font();
	void release_font();

private:
	static int _lua_clone(lua_State* L);
	static int _lua_render_text(lua_State* L);

private:
	TTF_Font* m_font;
	std::string m_font_name;
	int m_font_size;
	int m_outline_size;
	int m_max_width;
	Colour m_colour;
	Style m_style;
	Alignment m_alignment;
};

#endif // DECK_ASSISTANT_DECK_FONT_H
