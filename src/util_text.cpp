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

#include "util_text.h"
#include "deck_logger.h"
#include "lua_helpers.h"
#include <algorithm>
#include <cassert>
#include <charconv>
#include <fstream>
#include <set>

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

void add_indent(std::string& target, int indent, bool pretty)
{
	if (pretty)
	{
		target += '\n';
		target.append(indent, ' ');
	}
}

bool is_convertible_to_json(lua_State* L, int idx)
{
	int const vtype = lua_type(L, idx);
	return vtype == LUA_TNIL || vtype == LUA_TBOOLEAN || vtype == LUA_TNUMBER || vtype == LUA_TSTRING || vtype == LUA_TTABLE;
}

void convert_to_json_impl(lua_State* L, int idx, std::string& target, std::set<void const*>& seen, bool pretty, int indent)
{
	lua_checkstack(L, lua_gettop(L) + 6);

	int const vtype = lua_type(L, idx);
	switch (vtype)
	{
		case LUA_TNONE:
		case LUA_TNIL:
			target += "null";
			break;

		case LUA_TBOOLEAN:
			{
				bool value  = lua_toboolean(L, idx);
				target     += (value ? "true" : "false");
			}
			break;

		case LUA_TNUMBER:
			{
				lua_Number value = lua_tonumber(L, idx);

				char buf[32];
				auto [end, ec] = std::to_chars(buf, buf + sizeof(buf), value, std::chars_format::fixed, 8);
				if (ec != std::errc())
				{
					target += "null";
				}
				else
				{
					bool const has_dot = std::find(buf, end, '.') != end;
					if (has_dot) // Should always be true, but just for safety that we don't destroy integers
					{
						while (end > buf && end[-1] == '0')
							--end;
						if (end[-1] == '.')
							--end;
					}
					target.append(buf, end - buf);
				}
			}
			break;

		case LUA_TSTRING:
			{
				std::string_view value  = LuaHelpers::to_string_view(L, idx);
				target                 += '"';

				std::size_t offset = 0;
				while (offset < value.size())
				{
					std::size_t next = value.find_first_of("\"\\/\b\f\n\r\t", offset);
					if (next == std::string_view::npos)
						break;

					if (next > offset)
						target.append(value, offset, next - offset);

					target += '\\';

					char const v = value[next];
					switch (v)
					{
						case '\b':
							target += 'b';
							break;
						case '\f':
							target += 'f';
							break;
						case '\n':
							target += 'n';
							break;
						case '\r':
							target += 'r';
							break;
						case '\t':
							target += 't';
							break;
						default:
							target += v;
							break;
					}
					offset = next + 1;
				}

				target.append(value, offset);
				target += '"';
			}
			break;

		case LUA_TTABLE:
			{
				idx = LuaHelpers::absidx(L, idx);

				void const* this_table_ptr = lua_topointer(L, idx);
				if (seen.contains(this_table_ptr))
				{
					DeckLogger::lua_log_message(L, DeckLogger::Level::Warning, "recursion detected, setting value to null");
					target.append("null", 4);
					break;
				}

				// Check if its an array or object
				lua_rawgeti(L, idx, 1);
				if (lua_type(L, -1) != LUA_TNIL)
				{
					// Is array

					target += '[';
					indent += 2;

					int raw_index = 1;
					while (lua_type(L, -1) != LUA_TNIL)
					{
						if (is_convertible_to_json(L, -1))
						{
							if (raw_index > 1)
								target += ',';

							seen.insert(this_table_ptr);
							add_indent(target, indent, pretty);
							convert_to_json_impl(L, -1, target, seen, pretty, indent);
							seen.erase(this_table_ptr);
						}

						lua_pop(L, 1);
						++raw_index;
						lua_rawgeti(L, idx, raw_index);
					}

					lua_pop(L, 1);

					indent -= 2;
					add_indent(target, indent, pretty);
					target += ']';
				}
				else
				{
					// Is object

					target += '{';
					indent += 2;

					std::vector<std::string_view> sorted_keys;
					sorted_keys.reserve(64);

					while (lua_next(L, idx))
					{
						if (lua_type(L, -2) == LUA_TSTRING && is_convertible_to_json(L, -1))
						{
							std::string_view key = LuaHelpers::to_string_view(L, -2);
							sorted_keys.push_back(key);
						}
						lua_pop(L, 1);
					}

					std::sort(sorted_keys.begin(), sorted_keys.end());

					bool first = true;
					for (std::string_view key : sorted_keys)
					{
						lua_pushlstring(L, key.data(), key.size());
						lua_pushvalue(L, -1);
						lua_gettable(L, -3);

						if (!first)
							target += ',';
						else
							first = false;

						add_indent(target, indent, pretty);
						convert_to_json_impl(L, -2, target, seen, pretty, indent);

						target += ':';
						if (pretty)
							target += ' ';

						seen.insert(this_table_ptr);
						convert_to_json_impl(L, -1, target, seen, pretty, indent);
						seen.erase(this_table_ptr);

						lua_pop(L, 2);
					}

					indent -= 2;

					if (!first)
						add_indent(target, indent, pretty);

					target += '}';
				}
			}
			break;

		default:
			{
				std::string_view value  = LuaHelpers::push_converted_to_string(L, idx);
				target                 += value;
				lua_pop(L, 1);
			}
			break;
	}
}

std::string_view convert_from_json_impl(lua_State* L, std::string_view const& input, std::size_t& offset)
{
	std::size_t const end = input.size();

	while (offset < end && input[offset] <= 32)
		++offset;

	if (offset == end)
		return "unexpected end of file, expected value";

	char const token = input[offset];

	if (token == '{')
	{
		lua_createtable(L, 0, 8);
		++offset;

		bool first = true;
		while (offset < end)
		{
			while (offset < end && input[offset] <= 32)
				++offset;

			if (offset == end)
				return "unexpected end of file, expected }";

			if (input[offset] == '}')
				break;

			if (!first)
			{
				if (input[offset] != ',')
					return "expected ,";
				++offset;
			}

			std::string_view err = convert_from_json_impl(L, input, offset);
			if (!err.empty())
				return err;

			if (lua_type(L, -1) != LUA_TSTRING)
				return "object key must be string";

			while (offset < end && input[offset] <= 32)
				++offset;

			if (offset == end)
				return "unexpected end of file, expected :";

			if (input[offset] != ':')
				return "expected :";

			++offset;

			err = convert_from_json_impl(L, input, offset);
			if (!err.empty())
				return err;

			first = false;
			lua_rawset(L, -3);
		}

		++offset;
	}
	else if (token == '[')
	{
		lua_createtable(L, 0, 8);
		++offset;

		int raw_index = 0;
		while (offset < end)
		{
			while (offset < end && input[offset] <= 32)
				++offset;

			if (offset == end)
				return "unexpected end of file, expected ]";

			if (input[offset] == ']')
				break;

			if (raw_index > 0)
			{
				if (input[offset] != ',')
					return "expected ,";
				++offset;
			}

			std::string_view err = convert_from_json_impl(L, input, offset);
			if (!err.empty())
				return err;

			++raw_index;
			lua_rawseti(L, -2, raw_index);
		}
		++offset;
	}
	else if (token == '"')
	{
		luaL_Buffer buf;
		luaL_buffinit(L, &buf);

		++offset;
		while (offset < end)
		{
			std::size_t next = offset;
			while (next < end)
			{
				if (input[next] == '"' || input[next] == '\\')
					break;

				++next;
			}

			if (next == end)
				return "unexpected end of file, expected \"";

			if (next > offset)
			{
				luaL_addlstring(&buf, input.data() + offset, next - offset);
				offset = next;
			}

			if (input[offset] == '"')
				break;

			++offset;

			char const v = input[offset];
			switch (v)
			{
				case '"':
				case '\\':
				case '/':
					luaL_addchar(&buf, v);
					break;
				case 'b':
					luaL_addchar(&buf, '\b');
					break;
				case 'f':
					luaL_addchar(&buf, '\f');
					break;
				case 'n':
					luaL_addchar(&buf, '\n');
					break;
				case 'r':
					luaL_addchar(&buf, '\r');
					break;
				case 't':
					luaL_addchar(&buf, '\t');
					break;
				default:
					luaL_addchar(&buf, '\\');
					luaL_addchar(&buf, v);
					break;
			}

			++offset;
		}

		++offset;
		luaL_pushresult(&buf);
	}
	else if (token == '-' || (token >= '0' && token <= '9'))
	{
		lua_Number number;
		auto [ptr, ec] = std::from_chars(input.data() + offset, input.data() + input.size(), number);
		if (ec != std::errc())
			return "invalid number literal";

		lua_pushnumber(L, number);
		offset = ptr - input.data();
	}
	else if (token == 't')
	{
		if (input.substr(offset, 4) != "true")
			return "invalid literal, expected true";

		lua_pushboolean(L, true);
		offset += 4;
	}
	else if (token == 'f')
	{
		if (input.substr(offset, 5) != "false")
			return "invalid literal, expected false";

		lua_pushboolean(L, false);
		offset += 5;
	}
	else if (token == 'n')
	{
		if (input.substr(offset, 4) != "null")
			return "invalid literal, expected null";

		lua_pushnil(L);
		offset += 4;
	}
	else
	{
		return "invalid character";
	}

	return std::string_view();
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

std::strong_ordering nocase_compare(std::string_view const& lhs, std::string_view const& rhs)
{
	std::size_t const slhs = lhs.size();
	std::size_t const srhs = rhs.size();

	if (lhs.data() != rhs.data())
	{
		std::size_t const smin = std::min(slhs, srhs);
		for (std::size_t i = 0; i < smin; ++i)
		{
			auto ordering = std::tolower(lhs[i]) <=> std::tolower(rhs[i]);
			if (ordering != std::strong_ordering::equal)
				return ordering;
		}
	}

	return slhs <=> srhs;
}

std::string convert_to_json(lua_State* L, int idx, bool pretty)
{
	std::set<void const*> seen;
	std::string result;
	result.reserve(1024);
	convert_to_json_impl(L, idx, result, seen, pretty, 0);
	return result;
}

std::string_view convert_from_json(lua_State* L, std::string_view const& input, std::size_t& offset)
{
	int top = lua_gettop(L);

	std::string_view err = convert_from_json_impl(L, input, offset);
	if (!err.empty())
		lua_settop(L, top);

	return err;
}

std::string load_file(std::filesystem::path const& path, std::string& err)
{
	assert(path.is_absolute() && "load_file requires an absolute path");

	std::ifstream fp(path.c_str(), std::ios::in | std::ios::binary);
	if (!fp.good())
	{
		err = "failed to open file for reading";
		return std::string();
	}

	fp.seekg(0, std::ios::end);
	auto file_size = fp.tellg();

	if (file_size == 0)
		return std::string();

	fp.seekg(0, std::ios::beg);

	std::string contents;
	contents.reserve(std::size_t(file_size) + 1);

	std::copy_n(std::istreambuf_iterator(fp), file_size, std::back_inserter(contents));
	return contents;
}

bool save_file(std::filesystem::path const& path, std::string_view const& input, std::string& err)
{
	assert(path.is_absolute() && "save_file requires an absolute path");

	if (input.empty())
	{
		std::error_code ec;
		if (!std::filesystem::remove(path, ec))
		{
			err = ec.message();
			return false;
		}
	}
	else
	{
		std::ofstream fp(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
		if (!fp.good())
		{
			err = "failed to open file for writing";
			return false;
		}

		fp.write(input.data(), input.size());
		fp.close();
	}

	return true;
}

bool append_to_file(std::filesystem::path const& path, std::string_view const& input, bool add_newline, std::string& err)
{
	assert(path.is_absolute() && "save_file requires an absolute path");

	if (input.empty())
		return true;

	// std::cout << "WOULD APPEND TO FILE: " << path.c_str() << std::endl;
	std::ofstream fp(path.c_str(), std::ios::out | std::ios::binary | std::ios::ate | std::ios::app);
	if (!fp.good())
	{
		err = "failed to open file for appending";
		return false;
	}

	fp.write(input.data(), input.size());

	if (add_newline)
	{
		constexpr char const nl = '\n';
		fp.write(&nl, 1);
	}

	fp.close();

	return true;
}

std::string_view trim(std::string_view const& str)
{
	char const* start = str.data();
	char const* end   = start + str.size();

	while (end > start && end[-1] <= 32)
		--end;

	while (start < end && start[0] <= 32)
		++start;

	return (start == end) ? std::string_view() : std::string_view(start, end);
}

std::vector<std::string_view> split(std::string_view const& str, std::string_view const& split_str, std::size_t max_parts)
{
	assert(!split_str.empty());
	char const split_size = split_str.size();
	char const split_char = split_size == 1 ? split_str[0] : 0;

	std::vector<std::string_view> result;

	if (max_parts != 0)
		result.reserve(max_parts);
	else
		result.reserve(64);

	std::size_t offset    = 0;
	std::size_t const end = str.size();
	while (offset <= end)
	{
		--max_parts;

		std::size_t next;
		if (max_parts == 0 || offset == end)
			next = std::string_view::npos;
		else if (split_char)
			next = str.find(split_char, offset);
		else
			next = str.find(split_str, offset);

		if (next == std::string_view::npos)
			next = end;

		result.push_back(str.substr(offset, next - offset));
		offset = next + split_size;
	}

	return result;
}

std::pair<std::string_view, std::string_view> split1(std::string_view const& str, std::string_view const& split_str, bool trim_parts)
{
	assert(!split_str.empty());
	char const split_size = split_str.size();

	std::size_t split = (split_size == 1) ? str.find(split_str[0]) : str.find(split_str);

	std::string_view key = str.substr(0, split);
	std::string_view value;

	if (split != std::string_view::npos)
		value = str.substr(split + split_size);

	return trim_parts ? std::make_pair(trim(key), trim(value)) : std::make_pair(key, value);
}

std::string join(std::vector<std::string_view> const& items, std::string_view const& join_str)
{
	if (items.empty())
		return std::string();

	std::size_t total_len = (items.size() - 1) * join_str.size();

	for (std::string_view const& item : items)
		total_len += item.size();

	std::string result;
	result.reserve(total_len + 1);

	bool first = true;
	for (std::string_view const& item : items)
	{
		if (first)
			first = false;
		else
			result += join_str;

		result += item;
	}

	return result;
}

std::string replace(std::string_view const& str, std::string_view const& from_str, std::string_view const& to_str)
{
	return join(split(str, from_str), to_str);
}

std::pair<std::string_view, std::size_t> for_each_split(std::string_view const& str, std::string_view const& split_str, SplitCallback const& callback)
{
	assert(!split_str.empty());
	char const split_size = split_str.size();
	char const split_char = split_size == 1 ? split_str[0] : 0;

	std::size_t counter   = 0;
	std::size_t offset    = 0;
	std::size_t const end = str.size();

	while (offset <= end)
	{
		std::size_t next;
		if (offset == end)
			next = std::string_view::npos;
		else if (split_char)
			next = str.find(split_char, offset);
		else
			next = str.find(split_str, offset);

		if (next == std::string_view::npos)
			next = end;

		std::string_view segment = str.substr(offset, next - offset);
		offset                   = next + split_size;

		if (callback(counter, segment))
			return std::make_pair(segment, offset);

		++counter;
	}

	return std::make_pair(std::string_view(), str.size());
}

HttpMessage parse_http_message(std::string_view const& buffer)
{
	HttpMessage msg;

	auto [start_line, data] = split1(buffer, "\r\n", false);
	if (start_line.empty() || data.empty())
	{
		if (buffer.size() > 256)
			msg.error = "Invalid HTTP start line";

		return msg;
	}

	{
		auto start_line_parts = split(start_line, " ", 3);
		if (start_line_parts.size() < 3)
		{
			msg.error = "Invalid HTTP start line";
			return msg;
		}

		if (start_line_parts[0].starts_with("HTTP/"))
		{
			// Response
			msg.http_version = start_line_parts[0];

			char const* first = start_line_parts[1].data();
			char const* last  = first + start_line_parts[1].size();
			auto [ptr, ec]    = std::from_chars(first, last, msg.response_status_code, 10);
			if (ec != std::errc())
			{
				msg.error = "Invalid HTTP status code";
				return msg;
			}

			msg.response_status_message = start_line_parts[2];
		}
		else
		{
			// Request
			msg.request_method = start_line_parts[0];
			msg.request_path   = start_line_parts[1];
			msg.http_version   = start_line_parts[2];
		}
	}

	auto [last_header, remainder] = for_each_split(data, "\r\n", [&](std::size_t index, std::string_view const& segment) -> bool {
		if (segment.empty())
			return true;

		auto [key, value] = split1(segment, " ", true);
		if (key.empty() || key.back() != ':')
		{
			msg.error = "Invalid HTTP header";
			return true;
		}

		msg.headers[key.substr(0, key.size() - 1)] = value;
		return false;
	});

	if (remainder <= data.size())
		msg.body_start = remainder + start_line.size() + 2;

	return msg;
}

} // namespace util
