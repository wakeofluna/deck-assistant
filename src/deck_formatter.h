#ifndef DECK_ASSISTANT_DECK_FORMATTER_H
#define DECK_ASSISTANT_DECK_FORMATTER_H

#include "lua_class.h"
#include "util_colour.h"
#include <SDL_ttf.h>
#include <string>

class DeckFormatter : public LuaClass<DeckFormatter>
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
	DeckFormatter();
	DeckFormatter(DeckFormatter const&);

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

#endif // DECK_ASSISTANT_DECK_FORMATTER_H
