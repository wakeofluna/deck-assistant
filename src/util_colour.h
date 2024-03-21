#ifndef DECK_ASSISTENT_UTIL_COLOUR_H
#define DECK_ASSISTENT_UTIL_COLOUR_H

#include "SDL_pixels.h"
#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>

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

#endif // DECK_ASSISTENT_UTIL_COLOUR_H
