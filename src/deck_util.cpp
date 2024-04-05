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

#include "deck_util.h"
#include "lua_helpers.h"
#include "util_blob.h"
#include <algorithm>
#include <cassert>
#include <charconv>
#include <iostream>
#include <sstream>

namespace
{

void add_indent(std::ostream& stream, int indent, bool pretty)
{
	if (pretty)
	{
		stream << '\n';
		std::fill_n(std::ostreambuf_iterator<char>(stream), indent, ' ');
	}
}

bool is_convertible_to_json(lua_State* L, int idx)
{
	int const vtype = lua_type(L, idx);
	return vtype == LUA_TNIL || vtype == LUA_TBOOLEAN || vtype == LUA_TNUMBER || vtype == LUA_TSTRING || vtype == LUA_TTABLE;
}

void convert_to_json(lua_State* L, int idx, std::ostream& stream, bool pretty, int indent)
{
	lua_checkstack(L, lua_gettop(L) + 6);

	int const vtype = lua_type(L, idx);
	switch (vtype)
	{
		case LUA_TNONE:
		case LUA_TNIL:
			stream << "null";
			break;

		case LUA_TBOOLEAN:
			{
				bool value = lua_toboolean(L, idx);
				stream << (value ? "true" : "false");
			}
			break;

		case LUA_TNUMBER:
			{
				lua_Number value = lua_tonumber(L, idx);

				char buf[32];
				auto [end, ec] = std::to_chars(buf, buf + sizeof(buf), value, std::chars_format::fixed);
				if (ec != std::errc())
					stream << "null";
				else
					stream.write(buf, end - buf);
			}
			break;

		case LUA_TSTRING:
			{
				std::string_view value = LuaHelpers::to_string_view(L, idx);
				stream << '"';

				std::size_t offset = 0;
				while (offset < value.size())
				{
					std::size_t next = value.find_first_of("\\\"", offset);
					if (next == std::string_view::npos)
						break;

					if (next > offset)
						stream << value.substr(offset, next - offset);

					stream << '\\';
					stream << value[next];
					offset = next + 1;
				}

				stream << value.substr(offset);
				stream << '"';
			}
			break;

		case LUA_TTABLE:
			idx = LuaHelpers::absidx(L, idx);

			// Check if its an array or object
			lua_rawgeti(L, idx, 1);
			if (lua_type(L, -1) != LUA_TNIL)
			{
				// Is array

				stream << '[';
				indent += 2;

				int raw_index = 1;
				while (lua_type(L, -1) != LUA_TNIL)
				{
					if (is_convertible_to_json(L, -1))
					{
						if (raw_index > 1)
							stream << ',';

						add_indent(stream, indent, pretty);
						convert_to_json(L, -1, stream, pretty, indent);
					}

					lua_pop(L, 1);
					++raw_index;
					lua_rawgeti(L, idx, raw_index);
				}

				lua_pop(L, 1);

				indent -= 2;
				add_indent(stream, indent, pretty);
				stream << ']';
			}
			else
			{
				// Is object

				stream << '{';
				indent += 2;

				bool first = true;
				while (lua_next(L, idx))
				{
					if (lua_type(L, -2) == LUA_TSTRING && is_convertible_to_json(L, -1))
					{
						if (!first)
							stream << ',';
						else
							first = false;

						add_indent(stream, indent, pretty);

						convert_to_json(L, -2, stream, pretty, indent);

						stream << ':';
						if (pretty)
							stream << ' ';

						convert_to_json(L, -1, stream, pretty, indent);
					}

					lua_pop(L, 1);
				}

				indent -= 2;

				if (!first)
					add_indent(stream, indent, pretty);

				stream << '}';
			}
			break;

		default:
			{
				std::string_view value = LuaHelpers::push_converted_to_string(L, idx);
				stream << value;
				lua_pop(L, 1);
				break;
			}
	}
}

std::string_view convert_from_json(lua_State* L, std::string_view const& input, std::size_t& offset)
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

			std::string_view err = convert_from_json(L, input, offset);
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

			err = convert_from_json(L, input, offset);
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

			std::string_view err = convert_from_json(L, input, offset);
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

			if (input[offset] == '"')
				luaL_addchar(&buf, '"');
			else if (input[offset] == '\\')
				luaL_addchar(&buf, '\\');
			else if (input[offset] == 'n')
				luaL_addchar(&buf, '\n');
			else
				luaL_addlstring(&buf, input.data() + offset - 1, 2);

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

char const* DeckUtil::LUA_TYPENAME = "deck:DeckUtil";

void DeckUtil::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &_lua_from_base64);
	lua_setfield(L, -2, "from_base64");

	lua_pushcfunction(L, &_lua_to_base64);
	lua_setfield(L, -2, "to_base64");

	lua_pushcfunction(L, &_lua_from_hex);
	lua_setfield(L, -2, "from_hex");

	lua_pushcfunction(L, &_lua_to_hex);
	lua_setfield(L, -2, "to_hex");

	lua_pushcfunction(L, &_lua_from_json);
	lua_setfield(L, -2, "from_json");

	lua_pushcfunction(L, &_lua_to_json);
	lua_setfield(L, -2, "to_json");

	lua_pushcfunction(L, &_lua_sha1);
	lua_setfield(L, -2, "sha1");

	lua_pushcfunction(L, &_lua_sha256);
	lua_setfield(L, -2, "sha256");

	lua_pushcfunction(L, &_lua_random_bytes);
	lua_setfield(L, -2, "random_bytes");
}

int DeckUtil::newindex(lua_State* L)
{
	luaL_error(L, "%s instance is closed for modifications", type_name());
	return 0;
}

int DeckUtil::_lua_from_base64(lua_State* L)
{
	std::string_view input = LuaHelpers::to_string_view(L, 1);
	bool ok;

	Blob blob = Blob::from_base64(input, ok);
	if (!ok)
		luaL_error(L, "input is not valid base64");

	lua_pushlstring(L, (char const*)blob.data(), blob.size());
	return 1;
}

int DeckUtil::_lua_to_base64(lua_State* L)
{
	std::string_view input = LuaHelpers::to_string_view(L, 1);

	BlobView blob      = input;
	std::string output = blob.to_base64();

	lua_pushlstring(L, output.data(), output.size());
	return 1;
}

int DeckUtil::_lua_from_hex(lua_State* L)
{
	std::string_view input = LuaHelpers::to_string_view(L, 1);
	bool ok;

	Blob blob = Blob::from_hex(input, ok);
	if (!ok)
		luaL_error(L, "input is not valid hexadecimal");

	lua_pushlstring(L, (char const*)blob.data(), blob.size());
	return 1;
}

int DeckUtil::_lua_to_hex(lua_State* L)
{
	std::string_view input = LuaHelpers::to_string_view(L, 1);

	BlobView blob      = input;
	std::string output = blob.to_hex();

	lua_pushlstring(L, output.data(), output.size());
	return 1;
}

int DeckUtil::_lua_from_json(lua_State* L)
{
	std::string_view input = LuaHelpers::to_string_view(L, 1);
	std::size_t offset     = 0;

	std::string_view err = convert_from_json(L, input, offset);
	if (!err.empty())
	{
		luaL_Buffer buf;
		luaL_buffinit(L, &buf);

		std::string short_src;
		int currentline;
		LuaHelpers::lua_lineinfo(L, short_src, currentline);

		lua_pushfstring(L, "%s:%d: error at offset %d: ", short_src.c_str(), currentline, int(offset));
		luaL_addvalue(&buf);
		luaL_addlstring(&buf, err.data(), err.size());
		luaL_pushresult(&buf);
		lua_error(L);
	}

	return 1;
}

int DeckUtil::_lua_to_json(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	bool pretty = false;
	if (lua_type(L, 2) == LUA_TBOOLEAN)
		pretty = lua_toboolean(L, 2);

	lua_settop(L, 1);

	std::string buf;
	buf.reserve(1024);

	std::stringstream stream(std::move(buf));

	convert_to_json(L, 1, stream, pretty, 0);
	buf = std::move(stream).str();

	lua_pushlstring(L, buf.data(), buf.size());
	return 1;
}

int DeckUtil::_lua_sha1(lua_State* L)
{
	std::string_view input = LuaHelpers::to_string_view(L, 1);

#ifdef HAVE_GNUTLS
	BlobView blob = input;
	Blob output   = blob.sha1();

	lua_pushlstring(L, (char const*)output.data(), output.size());
	return 1;
#else
	(void)input;
	luaL_error(L, "sha1() function not implemented, recompile with gnutls support");
	return 0;
#endif
}

int DeckUtil::_lua_sha256(lua_State* L)
{
	std::string_view input = LuaHelpers::to_string_view(L, 1);

#ifdef HAVE_GNUTLS
	BlobView blob = input;
	Blob output   = blob.sha256();

	lua_pushlstring(L, (char const*)output.data(), output.size());
	return 1;
#else
	(void)input;
	luaL_error(L, "sha256() function not implemented, recompile with gnutls support");
	return 0;
#endif
}

int DeckUtil::_lua_random_bytes(lua_State* L)
{
	lua_Integer count = LuaHelpers::check_arg_int(L, 1);

	if (count <= 0)
		luaL_argerror(L, 1, "count must be larger than zero");

	Blob output = Blob::from_random(count);
	lua_pushlstring(L, (char const*)output.data(), output.size());
	return 1;
}
