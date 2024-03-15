#include "util_colour.h"

namespace
{

char nibble_to_hex(unsigned char nibble)
{
	if (nibble < 10)
		return '0' + nibble;
	else
		return 'a' + (nibble - 10);
}

} // namespace

std::string_view Colour::to_string(std::array<char, 10>& buffer) const
{
	buffer[0] = '#';
	buffer[1] = nibble_to_hex(r >> 4);
	buffer[2] = nibble_to_hex(r & 15);
	buffer[3] = nibble_to_hex(g >> 4);
	buffer[4] = nibble_to_hex(g & 15);
	buffer[5] = nibble_to_hex(b >> 4);
	buffer[6] = nibble_to_hex(b & 15);

	if (a == 255)
	{
		buffer[7] = 0;
		return std::string_view(buffer.data(), 7);
	}

	buffer[7] = nibble_to_hex(a >> 4);
	buffer[8] = nibble_to_hex(a & 15);
	buffer[9] = 0;
	return std::string_view(buffer.data(), 9);
}
