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
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace util
{

using SplitCallback = std::function<bool(std::size_t idx, std::string_view const&)>;

unsigned char hex_to_char(char const* hex);
unsigned char hex_to_char(char const* hex, bool& ok);
void char_to_hex(unsigned char ch, char* hex);
void char_to_hex_uc(unsigned char ch, char* hex);

std::strong_ordering nocase_compare(std::string_view const& lhs, std::string_view const& rhs);
inline bool nocase_equals(std::string_view const& lhs, std::string_view const& rhs)
{
	return nocase_compare(lhs, rhs) == std::strong_ordering::equal;
}

std::string convert_to_json(lua_State* L, int idx, bool pretty = false);
std::string_view convert_from_json(lua_State* L, std::string_view const& input, std::size_t& offset);

std::string load_file(std::filesystem::path const& path, std::string& err);
bool save_file(std::filesystem::path const& path, std::string_view const& input, std::string& err);
bool append_to_file(std::filesystem::path const& path, std::string_view const& input, bool add_newline, std::string& err);

std::string_view trim(std::string_view const& str);
std::vector<std::string_view> split(std::string_view const& str, std::string_view const& split_str = "\n", std::size_t max_parts = 0);
std::pair<std::string_view, std::string_view> split1(std::string_view const& str, std::string_view const& split_str, bool trim_parts = true);
std::string join(std::vector<std::string_view> const& items, std::string_view const& join_str);
std::string replace(std::string_view const& str, std::string_view const& from_str, std::string_view const& to_str);

// Returns [ found, remainder_position ]
std::pair<std::string_view, std::size_t> for_each_split(std::string_view const& str, std::string_view const& split_str, SplitCallback const& callback);

struct HttpMessage
{
	std::string error;

	std::string_view request_method;
	std::string_view request_path;
	std::string_view http_version;
	int response_status_code = 0;
	std::string_view response_status_message;
	std::map<std::string_view, std::string_view> headers;
	std::size_t body_start = 0;

	inline operator bool() const { return body_start > 0; }
};

HttpMessage parse_http_message(std::string_view const& buffer);

} // namespace util

#endif // DECK_ASSISTANT_UTIL_TEXT_H
