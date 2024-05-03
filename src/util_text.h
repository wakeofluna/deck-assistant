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

#ifndef DECK_ASSISTANT_UTIL_TEXT_H
#define DECK_ASSISTANT_UTIL_TEXT_H

#include <filesystem>
#include <functional>
#include <lua.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace util
{

using SplitCallback = std::function<bool(std::size_t idx, std::string_view const&)>;

unsigned char hex_to_char(char const* hex);
unsigned char hex_to_char(char const* hex, bool& ok);
void char_to_hex(unsigned char ch, char* hex);
void char_to_hex_uc(unsigned char ch, char* hex);

std::string convert_to_json(lua_State* L, int idx, bool pretty = false);
std::string_view convert_from_json(lua_State* L, std::string_view const& input, std::size_t& offset);

std::string load_file(std::filesystem::path const& path);

std::string_view trim(std::string_view const& str);
std::vector<std::string_view> split(std::string_view const& str, char split_char = '\n', std::size_t max_splits = std::size_t(-1));
std::string_view for_each_split(std::string_view const& str, char split_char, SplitCallback const& callback);

} // namespace util

#endif // DECK_ASSISTANT_UTIL_TEXT_H
