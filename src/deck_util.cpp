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
#include "util_paths.h"
#include <algorithm>
#include <cassert>
#include <charconv>
#include <chrono>
#include <fstream>
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
					std::size_t next = value.find_first_of("\"\\/\b\f\n\r\t", offset);
					if (next == std::string_view::npos)
						break;

					if (next > offset)
						stream << value.substr(offset, next - offset);

					stream << '\\';

					char const v = value[next];
					switch (v)
					{
						case '\b':
							stream << 'b';
							break;
						case '\f':
							stream << 'f';
							break;
						case '\n':
							stream << 'n';
							break;
						case '\r':
							stream << 'r';
							break;
						case '\t':
							stream << 't';
							break;
						default:
							stream << v;
							break;
					}
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

using SettingPair  = std::pair<std::string_view, std::string_view>;
using SettingPairs = std::vector<SettingPair>;

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

SettingPairs parse_settings(std::string_view const& data)
{
	SettingPairs result;
	result.reserve(16);

	std::size_t offset    = 0;
	std::size_t const end = data.size();

	while (offset < end)
	{
		std::size_t found = data.find('\n', offset);
		if (found == std::string_view::npos)
			found = end;

		std::string_view line = trim(data.substr(offset, found - offset));
		offset                = found + 1;

		if (line.empty() || line.starts_with('#'))
			continue;

		found = line.find('=');
		if (found == std::string_view::npos)
			continue;

		std::string_view key   = trim(line.substr(0, found));
		std::string_view value = trim(line.substr(found + 1));

		result.emplace_back(key, value);
	}

	return result;
}

bool store_settings(std::filesystem::path const& path, SettingPairs const& settings)
{
	assert(path.is_absolute() && "store_settings requires an absolute path");

	std::ofstream fp(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
	if (!fp.good())
		return false;

	for (SettingPair const& pair : settings)
	{
		if (!pair.first.empty() && !pair.second.empty())
		{
			fp.write(pair.first.data(), pair.first.size());
			fp.write(" = ", 3);
			fp.write(pair.second.data(), pair.second.size());
			fp.write("\n", 1);
		}
	}

	return true;
}

std::string load_file(std::filesystem::path const& path)
{
	assert(path.is_absolute() && "load_file requires an absolute path");

	std::ifstream fp(path.c_str(), std::ios::in | std::ios::binary);
	if (!fp.good())
		return std::string();

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

} // namespace

char const* DeckUtil::LUA_TYPENAME = "deck:DeckUtil";

DeckUtil::DeckUtil(LuaHelpers::Trust trust, Paths const* paths)
    : m_paths(paths)
    , m_trust(trust)
{
}

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

void DeckUtil::init_instance_table(lua_State* L)
{
	switch (m_trust)
	{
		case LuaHelpers::Trust::Trusted:
			lua_pushliteral(L, "Trusted");
			break;
		case LuaHelpers::Trust::Untrusted:
			lua_pushliteral(L, "Untrusted");
			break;
		case LuaHelpers::Trust::Admin:
			lua_pushliteral(L, "Admin");
			break;
	}
	lua_setfield(L, -2, "trust");

	lua_pushlightuserdata(L, (void*)m_paths);
	lua_pushinteger(L, int(m_trust));
	lua_pushcclosure(L, &_lua_ls, 2);
	lua_setfield(L, -2, "ls");

	lua_pushlightuserdata(L, (void*)m_paths);
	lua_pushcclosure(L, &_lua_store_secret, 1);
	lua_setfield(L, -2, "store_secret");

	lua_pushlightuserdata(L, (void*)m_paths);
	lua_pushinteger(L, int(m_trust));
	lua_pushcclosure(L, &_lua_retrieve_secret, 2);
	lua_setfield(L, -2, "retrieve_secret");
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

#if (defined HAVE_GNUTLS || defined HAVE_OPENSSL)
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

#if (defined HAVE_GNUTLS || defined HAVE_OPENSSL)
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

int DeckUtil::_lua_store_secret(lua_State* L)
{
	Paths const* paths = reinterpret_cast<Paths const*>(lua_touserdata(L, lua_upvalueindex(1)));

	std::string_view key   = LuaHelpers::check_arg_string(L, 1);
	std::string_view value = LuaHelpers::check_arg_string(L, 2);

	// TODO check key and value for invalid/dangerous characters

	std::filesystem::path path = paths->get_sandbox_dir() / "secrets.conf";
	std::string file_data      = load_file(path);
	SettingPairs settings      = parse_settings(file_data);

	bool found = false;
	for (SettingPair& pair : settings)
	{
		if (pair.first == key)
		{
			if (pair.second == value)
				return 0;

			pair.second = value;
			found       = true;
			break;
		}
	}

	if (!found)
		settings.emplace_back(key, value);

	store_settings(path, settings);
	return 0;
}

int DeckUtil::_lua_retrieve_secret(lua_State* L)
{
	Paths const* paths            = reinterpret_cast<Paths const*>(lua_touserdata(L, lua_upvalueindex(1)));
	LuaHelpers::Trust const trust = static_cast<LuaHelpers::Trust>(lua_tointeger(L, lua_upvalueindex(2)));

	std::string_view key = LuaHelpers::check_arg_string(L, 1);

	for (std::filesystem::path const& base_path : { paths->get_sandbox_dir(), paths->get_user_config_dir() })
	{
		std::filesystem::path path = base_path / "secrets.conf";
		std::string file_data      = load_file(path);
		SettingPairs settings      = parse_settings(file_data);

		for (SettingPair const& pair : settings)
		{
			if (pair.first == key)
			{
				lua_pushlstring(L, pair.second.data(), pair.second.size());
				return 1;
			}
		}

		if (trust == LuaHelpers::Trust::Untrusted)
			break;
	}

	return 0;
}

int DeckUtil::_lua_ls(lua_State* L)
{
	Paths const* paths            = reinterpret_cast<Paths const*>(lua_touserdata(L, lua_upvalueindex(1)));
	LuaHelpers::Trust const trust = static_cast<LuaHelpers::Trust>(lua_tointeger(L, lua_upvalueindex(2)));

	std::string_view request = LuaHelpers::check_arg_string_or_none(L, 1);
	std::filesystem::path request_path(request.empty() ? "." : request);

	// Trust rules:
	// Untrusted: sandbox only, no symlinks escape
	// Trusted: sandbox only, but symlinks can escape
	// Admin: no restrictions

	std::filesystem::path const& sandbox = paths->get_sandbox_dir();
	std::filesystem::path normal_path;
	if (request_path.is_absolute())
	{
		if (trust != LuaHelpers::Trust::Admin)
			luaL_error(L, "absolute paths not allowed");

		normal_path = request_path.lexically_normal();
	}
	else
	{
		normal_path = (sandbox / request_path).lexically_normal();
	}

	std::error_code ec;
	std::filesystem::path abs_path = std::filesystem::absolute(normal_path, ec);
	if (ec)
		luaL_error(L, "path error");
	abs_path.make_preferred();

	std::filesystem::path canon_path = std::filesystem::weakly_canonical(abs_path, ec);
	if (ec)
		luaL_error(L, "path error");
	canon_path.make_preferred();

	bool const canon_path_contained = Paths::verify_path_contains_path(canon_path, sandbox);
	if (trust == LuaHelpers::Trust::Untrusted && !canon_path_contained)
		luaL_error(L, "access denied");

	bool const abs_path_contained = Paths::verify_path_contains_path(abs_path, sandbox);
	if (trust != LuaHelpers::Trust::Admin && !abs_path_contained)
		luaL_error(L, "access denied");

	std::filesystem::file_status dir_status = std::filesystem::status(canon_path, ec);
	if (ec)
		luaL_error(L, "path does not exist or not readable");

	if (!std::filesystem::is_directory(dir_status))
		luaL_error(L, "not a directory");

	lua_createtable(L, 0, 6);

	if (request.empty())
		lua_pushlstring(L, ".", 1);
	else
		lua_pushvalue(L, 1);
	lua_setfield(L, -2, "path");

	if (abs_path_contained || canon_path_contained)
	{
		std::filesystem::path rel_path = (abs_path_contained ? abs_path : canon_path).lexically_relative(sandbox);
		lua_pushstring(L, rel_path.generic_string().c_str());
		lua_setfield(L, -2, "relative");
	}

	if (trust == LuaHelpers::Trust::Admin)
	{
		lua_pushstring(L, abs_path.generic_string().c_str());
		lua_setfield(L, -2, "absolute");

		lua_pushstring(L, canon_path.generic_string().c_str());
		lua_setfield(L, -2, "canonical");
	}

	std::filesystem::directory_iterator dir_end;
	std::filesystem::directory_iterator dir_iter(canon_path, std::filesystem::directory_options::skip_permission_denied, ec);
	if (ec)
		luaL_error(L, "path error");

	struct DirEntry
	{
		std::string name;

		std::strong_ordering operator<=>(DirEntry const& rhs) const { return name <=> rhs.name; }
		bool operator==(DirEntry const& rhs) const { return name == rhs.name; }
	};

	struct FileEntry
	{
		std::string name;
		std::uintmax_t size;
		std::filesystem::file_time_type mtime;

		std::strong_ordering operator<=>(FileEntry const& rhs) const { return name <=> rhs.name; }
		bool operator==(FileEntry const& rhs) const { return name == rhs.name; }
	};

	std::vector<DirEntry> subdirs;
	std::vector<FileEntry> files;
	subdirs.reserve(16);
	files.reserve(64);

	for (; dir_iter != dir_end; ++dir_iter)
	{
		if (trust == LuaHelpers::Trust::Untrusted)
		{
			std::filesystem::file_status sym_status = dir_iter->symlink_status(ec);
			if (ec || std::filesystem::is_symlink(sym_status))
				continue;
		}

		std::filesystem::file_status file_status = dir_iter->status(ec);
		if (ec)
			continue;

		if (std::filesystem::is_directory(file_status))
		{
			std::string fname = dir_iter->path().filename();
			if (!fname.starts_with('.'))
				subdirs.emplace_back(std::move(fname));
		}
		else if (std::filesystem::is_regular_file(file_status))
		{
			std::string fname = dir_iter->path().filename();
			if (!fname.starts_with('.'))
				files.emplace_back(std::move(fname), dir_iter->file_size(), dir_iter->last_write_time());
		}
	}

	std::ranges::sort(subdirs.begin(), subdirs.end());
	std::ranges::sort(files.begin(), files.end());

	lua_createtable(L, subdirs.size(), 0);
	for (std::size_t i = 0; i < subdirs.size(); ++i)
	{
		DirEntry const& entry = subdirs[i];
		lua_pushlstring(L, entry.name.data(), entry.name.size());
		lua_rawseti(L, -2, i + 1);
	}
	lua_setfield(L, -2, "subdirs");

	lua_createtable(L, files.size(), 0);
	for (std::size_t i = 0; i < files.size(); ++i)
	{
		FileEntry const& entry = files[i];
		auto file_time         = std::chrono::clock_cast<std::chrono::system_clock>(entry.mtime);
		auto epoch_time        = std::chrono::duration_cast<std::chrono::seconds>(file_time.time_since_epoch());
		lua_createtable(L, 3, 0);
		lua_pushlstring(L, entry.name.data(), entry.name.size());
		lua_rawseti(L, -2, 1);
		lua_pushinteger(L, entry.size);
		lua_rawseti(L, -2, 2);
		lua_pushinteger(L, epoch_time.count());
		lua_rawseti(L, -2, 3);
		lua_rawseti(L, -2, i + 1);
	}
	lua_setfield(L, -2, "files");

	return 1;
}
