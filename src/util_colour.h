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
	constexpr Colour(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a = 255)
	    : a(_a)
	    , r(_r)
	    , g(_g)
	    , b(_b)
	{
	}

	static bool parse_colour(std::string_view const& value, Colour& target);
	std::string_view to_string(std::array<char, 10>& buffer) const;

	constexpr inline void clear() { value = 0; }
	constexpr inline void set_pink()
	{
		a = 0xFF;
		r = 0xEE;
		g = 0x82;
		b = 0xEE;
	}

	constexpr inline operator SDL_Color() const
	{
		return SDL_Color { .r = r, .g = g, .b = b, .a = a };
	}

	union
	{
		struct
		{
			unsigned char a;
			unsigned char r;
			unsigned char g;
			unsigned char b;
		};
		std::uint32_t value;
	};
};

// Ensure compatibility with SDL
static_assert(sizeof(Colour) == 4);
static_assert(std::is_trivial_v<Colour>);
static_assert(std::is_standard_layout_v<Colour>);

#endif // DECK_ASSISTENT_UTIL_COLOUR_H
