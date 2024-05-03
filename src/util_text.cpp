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

namespace
{

constexpr unsigned char hex_to_nibble(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	return 0;
}

constexpr unsigned char hex_to_nibble(char ch, bool& ok)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	ok = false;
	return 0;
}

constexpr char nibble_to_hex(unsigned char nibble)
{
	if (nibble < 10)
		return '0' + nibble;
	else
		return 'a' + (nibble - 10);
}

constexpr char nibble_to_hex_uc(unsigned char nibble)
{
	if (nibble < 10)
		return '0' + nibble;
	else
		return 'A' + (nibble - 10);
}

} // namespace

namespace util
{

unsigned char hex_to_char(char const* hex)
{
	return (hex_to_nibble(hex[0]) << 4) + hex_to_nibble(hex[1]);
}

unsigned char hex_to_char(char const* hex, bool& ok)
{
	return (hex_to_nibble(hex[0], ok) << 4) + hex_to_nibble(hex[1], ok);
}

void char_to_hex(unsigned char ch, char* hex)
{
	hex[0] = nibble_to_hex(ch >> 4);
	hex[1] = nibble_to_hex(ch & 0x0f);
}

void char_to_hex_uc(unsigned char ch, char* hex)
{
	hex[0] = nibble_to_hex_uc(ch >> 4);
	hex[1] = nibble_to_hex_uc(ch & 0x0f);
}

} // namespace util
