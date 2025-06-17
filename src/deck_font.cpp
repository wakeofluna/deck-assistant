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

#include "deck_font.h"
#include "builtins.h"
#include "deck_card.h"
#include "deck_colour.h"
#include "lua_helpers.h"
#include <optional>

namespace
{

constexpr DeckFont::Alignment const k_align_left   = DeckFont::Alignment::Left;
constexpr DeckFont::Alignment const k_align_center = DeckFont::Alignment::Center;
constexpr DeckFont::Alignment const k_align_right  = DeckFont::Alignment::Right;

constexpr DeckFont::Style const k_style_regular       = DeckFont::Style::Regular;
constexpr DeckFont::Style const k_style_bold          = DeckFont::Style::Bold;
constexpr DeckFont::Style const k_style_italic        = DeckFont::Style::Italic;
constexpr DeckFont::Style const k_style_underline     = DeckFont::Style::Underline;
constexpr DeckFont::Style const k_style_strikethrough = DeckFont::Style::Strikethrough;

constexpr std::string_view to_string(DeckFont::Alignment alignment)
{
	switch (alignment)
	{
		case DeckFont::Alignment::Left:
			return "ALIGN_LEFT";
		case DeckFont::Alignment::Center:
			return "ALIGN_CENTER";
		case DeckFont::Alignment::Right:
			return "ALIGN_RIGHT";
	}
	return std::string_view("INTERNAL_ERROR");
}

constexpr std::string_view to_string(DeckFont::Style style)
{
	switch (style)
	{
		case DeckFont::Style::Regular:
			return "STYLE_REGULAR";
		case DeckFont::Style::Bold:
			return "STYLE_BOLD";
		case DeckFont::Style::Italic:
			return "STYLE_ITALIC";
		case DeckFont::Style::Underline:
			return "STYLE_UNDERLINE";
		case DeckFont::Style::Strikethrough:
			return "STYLE_STRIKETHROUGH";
	}
	return std::string_view("INTERNAL_ERROR");
}

constexpr std::optional<DeckFont::Alignment> to_alignment(void* ptr)
{
	if (ptr == &k_align_left)
		return DeckFont::Alignment::Left;
	else if (ptr == &k_align_center)
		return DeckFont::Alignment::Center;
	else if (ptr == &k_align_right)
		return DeckFont::Alignment::Right;
	else
		return std::nullopt;
}

constexpr std::optional<DeckFont::Style> to_style(void* ptr)
{
	if (ptr == &k_style_regular)
		return DeckFont::Style::Regular;
	else if (ptr == &k_style_bold)
		return DeckFont::Style::Bold;
	else if (ptr == &k_style_italic)
		return DeckFont::Style::Italic;
	else if (ptr == &k_style_underline)
		return DeckFont::Style::Underline;
	else if (ptr == &k_style_strikethrough)
		return DeckFont::Style::Strikethrough;
	else
		return std::nullopt;
}

constexpr int to_ttf_alignment(DeckFont::Alignment alignment)
{
	switch (alignment)
	{
		case DeckFont::Alignment::Left:
			return TTF_WRAPPED_ALIGN_LEFT;
		case DeckFont::Alignment::Center:
			return TTF_WRAPPED_ALIGN_CENTER;
		case DeckFont::Alignment::Right:
			return TTF_WRAPPED_ALIGN_RIGHT;
	}
	return TTF_WRAPPED_ALIGN_LEFT;
}

constexpr int to_ttf_style(DeckFont::Style style)
{
	switch (style)
	{
		case DeckFont::Style::Regular:
			return TTF_STYLE_NORMAL;
		case DeckFont::Style::Bold:
			return TTF_STYLE_BOLD;
		case DeckFont::Style::Italic:
			return TTF_STYLE_ITALIC;
		case DeckFont::Style::Underline:
			return TTF_STYLE_UNDERLINE;
		case DeckFont::Style::Strikethrough:
			return TTF_STYLE_STRIKETHROUGH;
	}
	return TTF_STYLE_NORMAL;
}

} // namespace

char const* DeckFont::LUA_TYPENAME = "deck:Font";

DeckFont::DeckFont()
    : m_font(nullptr)
    , m_font_size(12)
    , m_outline_size(0)
    , m_max_width(0)
    , m_colour(0, 0, 0)
    , m_style(Style::Regular)
    , m_alignment(Alignment::Left)
{
}

DeckFont::DeckFont(DeckFont const& other)
{
	m_font         = nullptr;
	m_font_name    = other.m_font_name;
	m_outline_size = other.m_outline_size;
	m_max_width    = other.m_max_width;
	m_colour       = other.m_colour;
	m_style        = other.m_style;
	m_alignment    = other.m_alignment;
}

void DeckFont::insert_enum_values(lua_State* L)
{
	for (auto k : { &k_align_left, &k_align_center, &k_align_right })
	{
		std::string_view name = to_string(*k);
		lua_pushlstring(L, name.data(), name.size());
		lua_pushlightuserdata(L, (void*)k);
		lua_settable(L, -3);
	}

	for (auto& k : { &k_style_regular, &k_style_bold, &k_style_italic, &k_style_underline, &k_style_strikethrough })
	{
		std::string_view name = to_string(*k);
		lua_pushlstring(L, name.data(), name.size());
		lua_pushlightuserdata(L, (void*)k);
		lua_settable(L, -3);
	}
}

void DeckFont::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &DeckFont::_lua_clone);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "clone");
	lua_setfield(L, -2, "dup");

	lua_pushcfunction(L, &DeckFont::_lua_render_text);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "Text");
	lua_setfield(L, -2, "render");
}

int DeckFont::index(lua_State* L, std::string_view const& key) const
{
	if (key == "font")
	{
		lua_pushlstring(L, m_font_name.c_str(), m_font_name.size());
	}
	else if (key == "size")
	{
		lua_pushinteger(L, m_font_size);
	}
	else if (key == "outline")
	{
		lua_pushinteger(L, m_outline_size);
	}
	else if (key == "max_width")
	{
		lua_pushinteger(L, m_max_width);
	}
	else if (key == "colour" || key == "color")
	{
		DeckColour::push_new(L, m_colour);
	}
	else if (key == "align" || key == "alignment")
	{
		switch (m_alignment)
		{
			case Alignment::Left:
				lua_pushlightuserdata(L, (void*)&k_align_left);
				break;
			case Alignment::Center:
				lua_pushlightuserdata(L, (void*)&k_align_center);
				break;
			case Alignment::Right:
				lua_pushlightuserdata(L, (void*)&k_align_right);
				break;
		}
	}
	else if (key == "style")
	{
		switch (m_style)
		{
			case Style::Regular:
				lua_pushlightuserdata(L, (void*)&k_style_regular);
				break;
			case Style::Bold:
				lua_pushlightuserdata(L, (void*)&k_style_bold);
				break;
			case Style::Italic:
				lua_pushlightuserdata(L, (void*)&k_style_italic);
				break;
			case Style::Underline:
				lua_pushlightuserdata(L, (void*)&k_style_underline);
				break;
			case Style::Strikethrough:
				lua_pushlightuserdata(L, (void*)&k_style_strikethrough);
				break;
		}
	}
	else
	{
		luaL_error(L, "invalid key for DeckFont (allowed: font, size, outline, max_width, colour, alignment, style)");
	}
	return 1;
}

int DeckFont::newindex(lua_State* L, lua_Integer key)
{
	int const vtype = lua_type(L, -1);
	if (vtype == LUA_TSTRING)
	{
		std::string_view value = LuaHelpers::to_string_view(L, -1);
		if (value != m_font_name)
		{
			release_font();
			m_font_name = value;
		}
	}
	else if (vtype == LUA_TNUMBER)
	{
		int value = lua_tointeger(L, -1);
		if (value != m_font_size)
		{
			release_font();
			m_font_size = value;
		}
	}
	else if (vtype == LUA_TLIGHTUSERDATA)
	{
		void* ptr = lua_touserdata(L, -1);
		if (auto alignment = to_alignment(ptr); alignment)
		{
			m_alignment = alignment.value();
		}
		else if (auto style = to_style(ptr); style)
		{
			if (style.value() != m_style)
			{
				m_style = style.value();
				if (m_font)
					TTF_SetFontStyle(m_font, to_ttf_style(m_style));
			}
		}
		else
		{
			luaL_argerror(L, 3, "unrecognised enum value for DeckFont");
		}
	}
	else if (DeckColour* colour = DeckColour::from_stack(L, -1, false); colour)
	{
		m_colour = colour->get_colour();
	}
	else
	{
		luaL_argerror(L, 3, "invalid argument for DeckFont");
	}

	return 0;
}

int DeckFont::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "font")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, -1);
		if (value != m_font_name)
		{
			release_font();
			m_font_name = value;
		}
	}
	else if (key == "size")
	{
		int value = LuaHelpers::check_arg_int(L, -1);
		if (value != m_font_size)
		{
			release_font();
			m_font_size = value;
		}
	}
	else if (key == "outline")
	{
		int value      = LuaHelpers::check_arg_int(L, -1);
		m_outline_size = value;
		if (m_font)
			TTF_SetFontOutline(m_font, m_outline_size);
	}
	else if (key == "max_width")
	{
		int value   = LuaHelpers::check_arg_int(L, -1);
		m_max_width = value;
	}
	else if (key == "colour" || key == "color")
	{
		DeckColour* colour = DeckColour::from_stack(L, -1);
		if (colour)
			m_colour = colour->get_colour();
	}
	else if (key == "align" || key == "alignment")
	{
		void* ptr = lua_touserdata(L, -1);
		if (auto alignment = to_alignment(ptr); alignment)
			m_alignment = alignment.value();
		else
			luaL_argerror(L, 3, "invalid enum value for alignment");
	}
	else if (key == "style")
	{
		void* ptr = lua_touserdata(L, -1);
		if (auto style = to_style(ptr); style)
		{
			if (style.value() != m_style)
			{
				m_style = style.value();
				if (m_font)
					TTF_SetFontStyle(m_font, to_ttf_style(m_style));
			}
		}
		else
		{
			luaL_argerror(L, 3, "invalid enum value for style");
		}
	}
	else
	{
		luaL_argerror(L, 2, "invalid key for DeckFont (allowed: font, size, outline, max_width, colour, alignment, style)");
	}
	return 0;
}

int DeckFont::tostring(lua_State* L) const
{
	luaL_Buffer buf;
	luaL_buffinit(L, &buf);

	std::array<char, 10> colbuf;

	std::string_view style     = to_string(m_style);
	std::string_view alignment = to_string(m_alignment);
	std::string_view colour    = m_colour.to_string(colbuf);

	lua_pushfstring(L, "%s { font='%s', size=%d, outline=%d, ", LUA_TYPENAME, m_font_name.c_str(), int(m_font_size), int(m_outline_size));
	luaL_addvalue(&buf);
	luaL_addlstring(&buf, style.data(), style.size());
	luaL_addlstring(&buf, " | ", 3);
	luaL_addlstring(&buf, alignment.data(), alignment.size());
	luaL_addlstring(&buf, ", ", 2);
	luaL_addlstring(&buf, colour.data(), colour.size());
	lua_pushfstring(L, ", max_width=%d }", int(m_max_width));
	luaL_addvalue(&buf);

	luaL_pushresult(&buf);
	return 1;
}

void DeckFont::load_font()
{
	if (!m_font)
	{
		SDL_RWops* src = builtins::as_rwops(builtins::font());
		m_font         = TTF_OpenFontRW(src, 1, m_font_size);

		TTF_SetFontOutline(m_font, m_outline_size);
		TTF_SetFontStyle(m_font, to_ttf_style(m_style));
		TTF_SetFontKerning(m_font, 1);
		TTF_SetFontHinting(m_font, TTF_HINTING_NORMAL);
	}
}

void DeckFont::release_font()
{
	if (m_font)
	{
		TTF_CloseFont(m_font);
		m_font = nullptr;
	}
}

int DeckFont::_lua_clone(lua_State* L)
{
	DeckFont const* self = from_stack(L, 1);

	(void)DeckFont::push_new(L, *self);

	if (lua_type(L, 2) == LUA_TTABLE)
	{
		lua_pushvalue(L, 2);
		LuaHelpers::copy_table_fields(L);
	}

	return 1;
}

int DeckFont::_lua_render_text(lua_State* L)
{
	DeckFont* self        = from_stack(L, 1);
	std::string_view text = LuaHelpers::check_arg_string(L, 2);

	util::Colour colour = self->m_colour;
	Alignment alignment = self->m_alignment;
	int max_width       = self->m_max_width;
	bool seen_max_width = false;

	for (int idx = 3; idx <= lua_gettop(L); ++idx)
	{
		int const vtype = lua_type(L, idx);

		if (vtype == LUA_TNUMBER)
		{
			if (!seen_max_width)
			{
				max_width      = lua_tointeger(L, idx);
				seen_max_width = true;
				continue;
			}
		}

		if (vtype == LUA_TLIGHTUSERDATA)
		{
			void* ptr = lua_touserdata(L, idx);
			if (auto override_alignment = to_alignment(ptr); override_alignment)
			{
				alignment = override_alignment.value();
				continue;
			}
		}

		if (DeckColour* override_colour = DeckColour::from_stack(L, idx, false); override_colour)
		{
			colour = override_colour->get_colour();
			continue;
		}

		if (vtype != LUA_TNIL)
			luaL_argerror(L, idx, "invalid override for DeckFont:render");
	}

	self->load_font();
	TTF_SetFontWrappedAlign(self->m_font, to_ttf_alignment(alignment));

	SDL_Surface* surface = TTF_RenderUTF8_Blended_Wrapped(self->m_font, text.data(), colour, max_width);
	if (!surface)
	{
		luaL_error(L, "error rendering text (invalid UTF8?)");
		return 0;
	}

	DeckCard::push_new(L, surface);

	// Store the text string for the user
	lua_pushvalue(L, 2);
	lua_setfield(L, -2, "text");

	return 1;
}
