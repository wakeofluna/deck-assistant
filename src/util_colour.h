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

#ifndef DECK_ASSISTENT_UTIL_COLOUR_H
#define DECK_ASSISTENT_UTIL_COLOUR_H

#include "SDL_pixels.h"
#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace util
{

struct Colour
{
	constexpr Colour() = default;
	constexpr Colour(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
	    : color { r, g, b, a }
	{
	}

	static bool parse_colour(std::string_view const& value, Colour& target);
	std::string_view to_string(std::array<char, 10>& buffer) const;

	constexpr inline void clear() { color = SDL_Color {}; }
	constexpr inline void set_pink()
	{
		color.r = 0xEE;
		color.g = 0x82;
		color.b = 0xEE;
		color.a = 0xFF;
	}

	constexpr inline operator SDL_Color() const
	{
		return color;
	}

	SDL_Color color;
};

// Ensure compatibility with SDL
static_assert(sizeof(Colour) == 4);
static_assert(std::is_trivial_v<Colour>);
static_assert(std::is_standard_layout_v<Colour>);

} // namespace util

#endif // DECK_ASSISTENT_UTIL_COLOUR_H
