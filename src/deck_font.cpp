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
#include "deck_enum.h"
#include "lua_helpers.h"
#include <optional>

namespace
{

constexpr std::string_view const k_enum_alignment = std::string_view("DeckFont::Alignment");
constexpr std::string_view const k_enum_style     = std::string_view("DeckFont::Style");

DeckEnum* k_align_left   = nullptr;
DeckEnum* k_align_center = nullptr;
DeckEnum* k_align_right  = nullptr;

DeckEnum* k_style_regular       = nullptr;
DeckEnum* k_style_bold          = nullptr;
DeckEnum* k_style_italic        = nullptr;
DeckEnum* k_style_underline     = nullptr;
DeckEnum* k_style_strikethrough = nullptr;

void register_enum_value(lua_State* L, DeckEnum* enum_value)
{
	std::string_view name = enum_value->value_name();
	lua_pushlstring(L, name.data(), name.size());
	enum_value->push_this(L);
	lua_settable(L, -3);
};

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
	m_font_size    = other.m_font_size;
	m_outline_size = other.m_outline_size;
	m_max_width    = other.m_max_width;
	m_colour       = other.m_colour;
	m_style        = other.m_style;
	m_alignment    = other.m_alignment;
}

void DeckFont::insert_enum_values(lua_State* L)
{
	auto get_or_create_alignment = [](lua_State* L, Alignment alignment) -> auto {
		DeckEnum* enum_value = DeckEnum::get_or_create(L, k_enum_alignment, to_string(alignment), alignment);
		register_enum_value(L, enum_value);
		return enum_value;
	};

	k_align_left   = get_or_create_alignment(L, Alignment::Left);
	k_align_center = get_or_create_alignment(L, Alignment::Center);
	k_align_right  = get_or_create_alignment(L, Alignment::Right);

	auto get_or_create_style = [](lua_State* L, Style style) -> auto {
		DeckEnum* enum_value = DeckEnum::get_or_create(L, k_enum_style, to_string(style), style);
		register_enum_value(L, enum_value);
		return enum_value;
	};

	k_style_regular       = get_or_create_style(L, Style::Regular);
	k_style_bold          = get_or_create_style(L, Style::Bold);
	k_style_italic        = get_or_create_style(L, Style::Italic);
	k_style_underline     = get_or_create_style(L, Style::Underline);
	k_style_strikethrough = get_or_create_style(L, Style::Strikethrough);
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
				k_align_left->push_this(L);
				break;
			case Alignment::Center:
				k_align_center->push_this(L);
				break;
			case Alignment::Right:
				k_align_right->push_this(L);
				break;
		}
	}
	else if (key == "style")
	{
		switch (m_style)
		{
			case Style::Regular:
				k_style_regular->push_this(L);
				break;
			case Style::Bold:
				k_style_bold->push_this(L);
				break;
			case Style::Italic:
				k_style_italic->push_this(L);
				break;
			case Style::Underline:
				k_style_underline->push_this(L);
				break;
			case Style::Strikethrough:
				k_style_strikethrough->push_this(L);
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
	else if (DeckEnum* enum_value = DeckEnum::from_stack(L, -1, false); enum_value)
	{
		if (std::optional<Alignment> align_value = enum_value->to_enum<Alignment>(k_enum_alignment); align_value)
		{
			m_alignment = align_value.value();
		}
		else if (std::optional<Style> style_value = enum_value->to_enum<Style>(k_enum_style); style_value)
		{
			if (m_style != style_value.value())
			{
				m_style = style_value.value();
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
		std::optional<Alignment> alignment = DeckEnum::to_enum<Alignment>(L, -1, k_enum_alignment);
		if (alignment)
			m_alignment = alignment.value();
	}
	else if (key == "style")
	{
		std::optional<Style> style = DeckEnum::to_enum<Style>(L, -1, k_enum_style);
		if (style)
		{
			if (m_style != style.value())
			{
				m_style = style.value();
				if (m_font)
					TTF_SetFontStyle(m_font, to_ttf_style(m_style));
			}
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

int DeckFont::to_ttf_alignment(DeckFont::Alignment alignment)
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

int DeckFont::to_ttf_style(DeckFont::Style style)
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

std::string_view DeckFont::to_string(DeckFont::Alignment alignment)
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

std::string_view DeckFont::to_string(DeckFont::Style style)
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

		if (vtype == LUA_TUSERDATA)
		{
			if (std::optional<Alignment> override_alignment = DeckEnum::to_enum<Alignment>(L, idx, k_enum_alignment, false); override_alignment)
			{
				alignment = override_alignment.value();
				continue;
			}

			if (DeckColour* override_colour = DeckColour::from_stack(L, idx, false); override_colour)
			{
				colour = override_colour->get_colour();
				continue;
			}
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
