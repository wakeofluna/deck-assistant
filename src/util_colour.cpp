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

#include "util_colour.h"
#include "util_text.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <utility>

using namespace util;

namespace
{

using StandardColour = std::pair<std::string_view, std::uint32_t>;

// HTML Standard Colours
constexpr StandardColour const g_standard_colours[] = {
	{"aliceblue",             0xF0F8FF},
	{ "antiquewhite",         0xFAEBD7},
	{ "aqua",	             0x00FFFF},
	{ "aquamarine",           0x7FFFD4},
	{ "azure",	            0xF0FFFF},
	{ "beige",	            0xF5F5DC},
	{ "bisque",               0xFFE4C4},
	{ "black",	            0x000000},
	{ "blanchedalmond",       0xFFEBCD},
	{ "blue",	             0x0000FF},
	{ "blueviolet",           0x8A2BE2},
	{ "brown",	            0xA52A2A},
	{ "burlywood",            0xDEB887},
	{ "cadetblue",            0x5F9EA0},
	{ "chartreuse",           0x7FFF00},
	{ "chocolate",            0xD2691E},
	{ "coral",	            0xFF7F50},
	{ "cornflowerblue",       0x6495ED},
	{ "cornsilk",             0xFFF8DC},
	{ "crimson",              0xDC143C},
	{ "cyan",	             0x00FFFF},
	{ "darkblue",             0x00008B},
	{ "darkcyan",             0x008B8B},
	{ "darkgoldenrod",        0xB8860B},
	{ "darkgray",             0xA9A9A9},
	{ "darkgreen",            0x006400},
	{ "darkgrey",             0xA9A9A9},
	{ "darkkhaki",            0xBDB76B},
	{ "darkmagenta",          0x8B008B},
	{ "darkolivegreen",       0x556B2F},
	{ "darkorange",           0xFF8C00},
	{ "darkorchid",           0x9932CC},
	{ "darkred",              0x8B0000},
	{ "darksalmon",           0xE9967A},
	{ "darkseagreen",         0x8FBC8F},
	{ "darkslateblue",        0x483D8B},
	{ "darkslategray",        0x2F4F4F},
	{ "darkslategrey",        0x2F4F4F},
	{ "darkturquoise",        0x00CED1},
	{ "darkviolet",           0x9400D3},
	{ "deeppink",             0xFF1493},
	{ "deepskyblue",          0x00BFFF},
	{ "dimgray",              0x696969},
	{ "dimgrey",              0x696969},
	{ "dodgerblue",           0x1E90FF},
	{ "firebrick",            0xB22222},
	{ "floralwhite",          0xFFFAF0},
	{ "forestgreen",          0x228B22},
	{ "fuchsia",              0xFF00FF},
	{ "gainsboro",            0xDCDCDC},
	{ "ghostwhite",           0xF8F8FF},
	{ "gold",	             0xFFD700},
	{ "goldenrod",            0xDAA520},
	{ "gray",	             0x808080},
	{ "green",	            0x008000},
	{ "greenyellow",          0xADFF2F},
	{ "grey",	             0x808080},
	{ "honeydew",             0xF0FFF0},
	{ "hotpink",              0xFF69B4},
	{ "indianred",            0xCD5C5C},
	{ "indigo",               0x4B0082},
	{ "ivory",	            0xFFFFF0},
	{ "khaki",	            0xF0E68C},
	{ "lavender",             0xE6E6FA},
	{ "lavenderblush",        0xFFF0F5},
	{ "lawngreen",            0x7CFC00},
	{ "lemonchiffon",         0xFFFACD},
	{ "lightblue",            0xADD8E6},
	{ "lightcoral",           0xF08080},
	{ "lightcyan",            0xE0FFFF},
	{ "lightgoldenrodyellow", 0xFAFAD2},
	{ "lightgray",            0xD3D3D3},
	{ "lightgreen",           0x90EE90},
	{ "lightgrey",            0xD3D3D3},
	{ "lightpink",            0xFFB6C1},
	{ "lightsalmon",          0xFFA07A},
	{ "lightseagreen",        0x20B2AA},
	{ "lightskyblue",         0x87CEFA},
	{ "lightslategray",       0x778899},
	{ "lightslategrey",       0x778899},
	{ "lightsteelblue",       0xB0C4DE},
	{ "lightyellow",          0xFFFFE0},
	{ "lime",	             0x00FF00},
	{ "limegreen",            0x32CD32},
	{ "linen",	            0xFAF0E6},
	{ "magenta",              0xFF00FF},
	{ "maroon",               0x800000},
	{ "mediumaquamarine",     0x66CDAA},
	{ "mediumblue",           0x0000CD},
	{ "mediumorchid",         0xBA55D3},
	{ "mediumpurple",         0x9370DB},
	{ "mediumseagreen",       0x3CB371},
	{ "mediumslateblue",      0x7B68EE},
	{ "mediumspringgreen",    0x00FA9A},
	{ "mediumturquoise",      0x48D1CC},
	{ "mediumvioletred",      0xC71585},
	{ "midnightblue",         0x191970},
	{ "mintcream",            0xF5FFFA},
	{ "mistyrose",            0xFFE4E1},
	{ "moccasin",             0xFFE4B5},
	{ "navajowhite",          0xFFDEAD},
	{ "navy",	             0x000080},
	{ "oldlace",              0xFDF5E6},
	{ "olive",	            0x808000},
	{ "olivedrab",            0x6B8E23},
	{ "orange",               0xFFA500},
	{ "orangered",            0xFF4500},
	{ "orchid",               0xDA70D6},
	{ "palegoldenrod",        0xEEE8AA},
	{ "palegreen",            0x98FB98},
	{ "paleturquoise",        0xAFEEEE},
	{ "palevioletred",        0xDB7093},
	{ "papayawhip",           0xFFEFD5},
	{ "peachpuff",            0xFFDAB9},
	{ "peru",	             0xCD853F},
	{ "pink",	             0xFFC0CB},
	{ "plum",	             0xDDA0DD},
	{ "powderblue",           0xB0E0E6},
	{ "purple",               0x800080},
	{ "rebeccapurple",        0x663399},
	{ "red",	              0xFF0000},
	{ "rosybrown",            0xBC8F8F},
	{ "royalblue",            0x4169E1},
	{ "saddlebrown",          0x8B4513},
	{ "salmon",               0xFA8072},
	{ "sandybrown",           0xF4A460},
	{ "seagreen",             0x2E8B57},
	{ "seashell",             0xFFF5EE},
	{ "sienna",               0xA0522D},
	{ "silver",               0xC0C0C0},
	{ "skyblue",              0x87CEEB},
	{ "slateblue",            0x6A5ACD},
	{ "slategray",            0x708090},
	{ "slategrey",            0x708090},
	{ "snow",	             0xFFFAFA},
	{ "springgreen",          0x00FF7F},
	{ "steelblue",            0x4682B4},
	{ "tan",	              0xD2B48C},
	{ "teal",	             0x008080},
	{ "thistle",              0xD8BFD8},
	{ "tomato",               0xFF6347},
	{ "turquoise",            0x40E0D0},
	{ "violet",               0xEE82EE},
	{ "wheat",	            0xF5DEB3},
	{ "white",	            0xFFFFFF},
	{ "whitesmoke",           0xF5F5F5},
	{ "yellow",               0xFFFF00},
	{ "yellowgreen",          0x9ACD32},
};
constexpr std::size_t const g_num_standard_colours = sizeof(g_standard_colours) / sizeof(*g_standard_colours);

constexpr bool standard_colour_search(StandardColour const& colour, std::string_view const& key)
{
	return colour.first < key;
}

constexpr bool standard_colours_are_sorted()
{
	for (std::size_t i = 1; i < g_num_standard_colours; ++i)
		if (!standard_colour_search(g_standard_colours[i - 1], g_standard_colours[i].first))
			return false;
	return true;
}

// Required for std::lower_bound to work correctly
static_assert(standard_colours_are_sorted());

bool parse_hex(std::string_view const& value, Colour& target)
{
	if (value.size() == 3)
	{
		char hex[2];
		hex[1] = 0;

		hex[0]         = value[0];
		target.color.r = hex_to_char(hex);
		hex[0]         = value[1];
		target.color.g = hex_to_char(hex);
		hex[0]         = value[2];
		target.color.b = hex_to_char(hex);
		target.color.a = 255;
		return true;
	}
	else if (value.size() == 6)
	{
		target.color.r = hex_to_char(&value[0]);
		target.color.g = hex_to_char(&value[2]);
		target.color.b = hex_to_char(&value[4]);
		target.color.a = 255;
		return true;
	}
	else if (value.size() == 8)
	{
		target.color.r = hex_to_char(&value[0]);
		target.color.g = hex_to_char(&value[2]);
		target.color.b = hex_to_char(&value[4]);
		target.color.a = hex_to_char(&value[6]);
		return true;
	}
	else
	{
		return false;
	}
}

} // namespace

void Colour::component_blend(Uint8& value, Uint8 const target, Uint32 ifactor)
{
	assert(ifactor >= 0 && ifactor <= 1024);

	Uint32 mod;
	if (target >= value)
	{
		mod   = (Uint32(target - value) * ifactor + 512) >> 10;
		value = std::clamp<int>(value + mod, 0, 255);
	}
	else
	{
		mod   = (Uint32(value - target) * ifactor) >> 10;
		value = std::clamp<int>(value - mod, 0, 255);
	}
}

void Colour::pixel_desaturate(Uint8& r, Uint8& g, Uint8& b, Uint32 ifactor)
{
	assert(ifactor >= 0 && ifactor <= 1024);

	unsigned int const luminance = 307 * r + 614 * g + 103 * b;
	unsigned int const scaled_r  = r << 10;
	unsigned int const scaled_g  = g << 10;
	unsigned int const scaled_b  = b << 10;

	r = ((scaled_r << 10) + ifactor * (luminance - scaled_r)) >> 20;
	g = ((scaled_g << 10) + ifactor * (luminance - scaled_g)) >> 20;
	b = ((scaled_b << 10) + ifactor * (luminance - scaled_b)) >> 20;
}

bool Colour::parse_colour(std::string_view const& value, Colour& target)
{
	if (value.size() == 0)
		return false;

	char str[32];
	size_t len = 0;
	for (; len < sizeof(str) && len < value.size(); ++len)
		str[len] = std::tolower(value[len]);
	std::string_view lc_value(str, len);

	if (lc_value[0] == '#')
		return parse_hex(lc_value.substr(1), target);

	StandardColour const* first = g_standard_colours;
	StandardColour const* last  = g_standard_colours + g_num_standard_colours;
	StandardColour const* found = std::lower_bound(first, last, lc_value, &standard_colour_search);
	if (found != last && found->first == lc_value)
	{
		target.color.r = (found->second >> 16) & 0xff;
		target.color.g = (found->second >> 8) & 0xff;
		target.color.b = (found->second >> 0) & 0xff;
		target.color.a = 255;
		return true;
	}
	else if (lc_value == "transparent")
	{
		target.color.r = 0;
		target.color.g = 0;
		target.color.b = 0;
		target.color.a = 0;
		return true;
	}
	else
	{
		return false;
	}
}

std::string_view Colour::to_string(std::array<char, 10>& buffer) const
{
	buffer[0] = '#';

	char_to_hex(color.r, &buffer[1]);
	char_to_hex(color.g, &buffer[3]);
	char_to_hex(color.b, &buffer[5]);

	if (color.a == 255)
	{
		buffer[7] = 0;
		return std::string_view(buffer.data(), 7);
	}

	char_to_hex(color.a, &buffer[7]);

	buffer[9] = 0;
	return std::string_view(buffer.data(), 9);
}
