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
#include "builtins.h"
#include "lua_helpers.h"
#include "util_blob.h"
#include "util_paths.h"
#include "util_text.h"
#include <SDL_clipboard.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// clang-format: do not reorder
#include <shellapi.h>
#else
#include <unistd.h>
#endif

namespace
{

using SettingPair  = std::pair<std::string_view, std::string_view>;
using SettingPairs = std::vector<SettingPair>;

SettingPairs parse_settings(std::string_view const& data)
{
	SettingPairs result;
	result.reserve(16);

	util::for_each_split(data, "\n", [&](std::size_t line_nr, std::string_view const& line) {
		std::string_view trimmed = util::trim(line);
		if (!trimmed.empty() && !trimmed.starts_with('#'))
		{
			auto [key, value] = util::split1(line, "=");
			if (!key.empty() && !value.empty())
				result.emplace_back(key, value);
		}
		return false;
	});

	return result;
}

bool store_settings(std::filesystem::path const& path, SettingPairs const& settings, std::string& err)
{
	assert(path.is_absolute() && "store_settings requires an absolute path");

	std::ofstream fp(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
	if (!fp.good())
	{
		err = "failed to open file for writing";
		return false;
	}

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

bool is_alphanumeric(std::string_view const& str, bool allow_empty = false)
{
	if (str.empty())
		return allow_empty;

	for (char ch : str)
	{
		if (ch >= 'a' && ch <= 'z')
			continue;
		if (ch >= 'A' && ch <= 'Z')
			continue;
		if (ch >= '0' && ch <= '9')
			continue;
		if (ch == '_')
			continue;
		return false;
	}

	return true;
}

bool is_ascii(std::string_view const& str, bool allow_empty = false)
{
	if (str.empty())
		return allow_empty;

	for (unsigned char ch : str)
	{
		if (ch >= 32 && ch < 128)
			continue;
		return false;
	}

	return true;
}

} // namespace

char const* DeckUtil::LUA_TYPENAME = "deck:DeckUtil";

DeckUtil::DeckUtil(LuaHelpers::Trust trust, util::Paths const& paths)
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

	lua_pushcfunction(L, &_lua_split_string);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "split");
	lua_setfield(L, -2, "split_string");

	lua_pushcfunction(L, &_lua_parse_http_message);
	lua_setfield(L, -2, "parse_http_message");

	lua_pushcfunction(L, &_lua_sha1);
	lua_setfield(L, -2, "sha1");

	lua_pushcfunction(L, &_lua_sha256);
	lua_setfield(L, -2, "sha256");

	lua_pushcfunction(L, &_lua_random_bytes);
	lua_setfield(L, -2, "random_bytes");

	lua_pushcfunction(L, &_lua_open_browser);
	lua_setfield(L, -2, "open_browser");

	lua_pushcfunction(L, &_lua_clipboard_text);
	lua_setfield(L, -2, "clipboard_text");

	lua_pushcfunction(L, &_lua_yieldable_call);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "yieldable_call");
	lua_setfield(L, -2, "ycall");
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

	lua_pushlightuserdata(L, (void*)&m_paths);
	lua_pushinteger(L, int(m_trust));
	lua_pushcclosure(L, &_lua_ls, 2);
	lua_setfield(L, -2, "ls");

	lua_pushlightuserdata(L, (void*)&m_paths);
	lua_pushcclosure(L, &_lua_store_secret, 1);
	lua_setfield(L, -2, "store_secret");

	lua_pushlightuserdata(L, (void*)&m_paths);
	lua_pushinteger(L, int(m_trust));
	lua_pushcclosure(L, &_lua_retrieve_secret, 2);
	lua_setfield(L, -2, "retrieve_secret");

	lua_pushlightuserdata(L, (void*)&m_paths);
	lua_pushcclosure(L, &_lua_store_table, 1);
	lua_setfield(L, -2, "store_table");

	lua_pushlightuserdata(L, (void*)&m_paths);
	lua_pushcclosure(L, &_lua_retrieve_table, 1);
	lua_setfield(L, -2, "retrieve_table");

	lua_pushlightuserdata(L, (void*)&m_paths);
	lua_pushcclosure(L, &_lua_append_event_log, 1);
	lua_setfield(L, -2, "append_event_log");

	lua_pushlightuserdata(L, (void*)&m_paths);
	lua_pushcclosure(L, &_lua_retrieve_event_log, 1);
	lua_setfield(L, -2, "retrieve_event_log");
}

int DeckUtil::index(lua_State* L, std::string_view const& key) const
{
	if (key == "svg_icon")
	{
		std::string_view icon = builtins::deck_assistant_icon();
		lua_pushlstring(L, icon.data(), icon.size());
		LuaHelpers::newindex_store_in_instance_table(L);
	}
	else if (key == "oauth2_page")
	{
		std::string_view page = builtins::oauth2_callback_page();
		lua_pushlstring(L, page.data(), page.size());
		LuaHelpers::newindex_store_in_instance_table(L);
	}
	return lua_gettop(L) == 2 ? 0 : 1;
}

int DeckUtil::newindex(lua_State* L)
{
	luaL_error(L, "%s instance is closed for modifications", type_name());
	return 0;
}

int DeckUtil::_lua_from_base64(lua_State* L)
{
	std::string_view input = LuaHelpers::check_arg_string(L, 1);
	bool ok;

	util::Blob blob = util::Blob::from_base64(input, ok);
	if (!ok)
		luaL_error(L, "input is not valid base64");

	lua_pushlstring(L, (char const*)blob.data(), blob.size());
	return 1;
}

int DeckUtil::_lua_to_base64(lua_State* L)
{
	std::string_view input = LuaHelpers::check_arg_string(L, 1);

	util::BlobView blob = input;
	std::string output  = blob.to_base64();

	lua_pushlstring(L, output.data(), output.size());
	return 1;
}

int DeckUtil::_lua_from_hex(lua_State* L)
{
	std::string_view input = LuaHelpers::check_arg_string(L, 1);
	bool ok;

	util::Blob blob = util::Blob::from_hex(input, ok);
	if (!ok)
		luaL_error(L, "input is not valid hexadecimal");

	lua_pushlstring(L, (char const*)blob.data(), blob.size());
	return 1;
}

int DeckUtil::_lua_to_hex(lua_State* L)
{
	std::string_view input = LuaHelpers::check_arg_string(L, 1);

	util::BlobView blob = input;
	std::string output  = blob.to_hex();

	lua_pushlstring(L, output.data(), output.size());
	return 1;
}

int DeckUtil::_lua_from_json(lua_State* L)
{
	std::string_view input = LuaHelpers::check_arg_string(L, 1);
	std::size_t offset     = 0;

	std::string_view err = util::convert_from_json(L, input, offset);
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
	luaL_checkany(L, 1);

	bool pretty = false;
	if (lua_type(L, 2) != LUA_TNONE)
		pretty = LuaHelpers::check_arg_bool(L, 2);

	lua_settop(L, 1);

	std::string result = util::convert_to_json(L, 1, pretty);
	lua_pushlstring(L, result.data(), result.size());
	return 1;
}

int DeckUtil::_lua_split_string(lua_State* L)
{
	std::string_view haystack = LuaHelpers::check_arg_string(L, 1);
	std::string_view needle   = LuaHelpers::check_arg_string(L, 2);
	bool filter_empty         = lua_toboolean(L, 3);

	lua_createtable(L, 8, 0);
	util::for_each_split(haystack, needle, [L, filter_empty](std::size_t index, std::string_view const& segment) -> bool {
		if (!filter_empty || !segment.empty())
		{
			lua_pushlstring(L, segment.data(), segment.size());
			lua_rawseti(L, -2, index + 1);
		}
		return false;
	});

	return 1;
}

int DeckUtil::_lua_parse_http_message(lua_State* L)
{
	std::string_view input = LuaHelpers::check_arg_string(L, 1);

	util::HttpMessage http_message = util::parse_http_message(input);

	lua_pushboolean(L, bool(http_message));
	lua_createtable(L, 0, 6);

	if (!http_message.http_version.empty())
	{
		lua_pushlstring(L, http_message.http_version.data(), http_message.http_version.size());
		lua_setfield(L, -2, "http_version");
	}

	lua_pushboolean(L, true);
	if (http_message.response_status_code > 0)
	{
		lua_setfield(L, -2, "response");
		lua_pushinteger(L, http_message.response_status_code);
		lua_setfield(L, -2, "code");
		lua_pushlstring(L, http_message.response_status_message.data(), http_message.response_status_message.size());
		lua_setfield(L, -2, "status");
	}
	else
	{
		lua_setfield(L, -2, "request");
		lua_pushlstring(L, http_message.request_method.data(), http_message.request_method.size());
		lua_setfield(L, -2, "method");
		lua_pushlstring(L, http_message.request_path.data(), http_message.request_path.size());
		lua_setfield(L, -2, "path");
	}

	lua_createtable(L, 0, http_message.headers.size());
	for (auto iter : http_message.headers)
	{
		lua_pushlstring(L, iter.first.data(), iter.first.size());
		lua_pushlstring(L, iter.second.data(), iter.second.size());
		lua_settable(L, -3);
	}
	lua_setfield(L, -2, "headers");

	if (http_message.body_start > 0)
	{
		lua_pushlstring(L, input.data() + http_message.body_start, input.size() - http_message.body_start);
		lua_setfield(L, -2, "body");
	}

	if (http_message.error.empty())
		return 2;

	lua_pushlstring(L, http_message.error.data(), http_message.error.size());
	return 3;
}

int DeckUtil::_lua_sha1(lua_State* L)
{
	std::string_view input = LuaHelpers::check_arg_string(L, 1);

#if (defined HAVE_GNUTLS || defined HAVE_OPENSSL)
	util::BlobView blob = input;
	util::Blob output   = blob.sha1();

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
	std::string_view input = LuaHelpers::check_arg_string(L, 1);

#if (defined HAVE_GNUTLS || defined HAVE_OPENSSL)
	util::BlobView blob = input;
	util::Blob output   = blob.sha256();

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

	util::Blob output = util::Blob::from_random(count);
	lua_pushlstring(L, (char const*)output.data(), output.size());
	return 1;
}

int DeckUtil::_lua_store_secret(lua_State* L)
{
	util::Paths const* paths = reinterpret_cast<util::Paths const*>(lua_touserdata(L, lua_upvalueindex(1)));

	std::string_view key   = LuaHelpers::check_arg_string(L, 1);
	std::string_view value = LuaHelpers::check_arg_string(L, 2, true);

	luaL_argcheck(L, is_alphanumeric(key), 1, "secret key must be alphanumeric");
	luaL_argcheck(L, is_ascii(value, true), 2, "secret value must be ascii");

	std::string err;
	std::filesystem::path path = paths->get_sandbox_dir() / "secrets.conf";
	std::string file_data      = util::load_file(path, err);

	if (!err.empty())
		luaL_error(L, "store secret failed: %s", err.c_str());

	SettingPairs settings = parse_settings(file_data);

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

	store_settings(path, settings, err);

	if (!err.empty())
		luaL_error(L, "store secret failed: %s", err.c_str());

	return 0;
}

int DeckUtil::_lua_retrieve_secret(lua_State* L)
{
	util::Paths const* paths      = reinterpret_cast<util::Paths const*>(lua_touserdata(L, lua_upvalueindex(1)));
	LuaHelpers::Trust const trust = static_cast<LuaHelpers::Trust>(lua_tointeger(L, lua_upvalueindex(2)));

	std::string_view key = LuaHelpers::check_arg_string(L, 1);

	for (std::filesystem::path const& base_path : { paths->get_sandbox_dir(), paths->get_user_config_dir() })
	{
		std::string err;
		std::filesystem::path path = base_path / "secrets.conf";
		std::string file_data      = util::load_file(path, err);

		if (!err.empty())
			continue;

		SettingPairs settings = parse_settings(file_data);

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

int DeckUtil::_lua_store_table(lua_State* L)
{
	util::Paths const* paths = reinterpret_cast<util::Paths const*>(lua_touserdata(L, lua_upvalueindex(1)));

	std::string_view store_name = LuaHelpers::check_arg_string(L, 1);
	luaL_argcheck(L, is_alphanumeric(store_name), 1, "store name must be alphanumeric");
	luaL_checktype(L, 2, LUA_TTABLE);

	std::string json = util::convert_to_json(L, 2, true);
	if (json.empty())
		luaL_error(L, "error converting table to json");

	std::string store_filename  = "table_";
	store_filename             += store_name;
	store_filename             += ".json";

	std::filesystem::path path = paths->get_sandbox_dir() / store_filename;

	std::string err;
	if (!util::save_file(path, json, err))
		luaL_error(L, "%s: error storing table: %s", store_filename.c_str(), err.c_str());

	return 0;
}

int DeckUtil::_lua_retrieve_table(lua_State* L)
{
	util::Paths const* paths = reinterpret_cast<util::Paths const*>(lua_touserdata(L, lua_upvalueindex(1)));

	std::string_view store_name = LuaHelpers::check_arg_string(L, 1);
	luaL_argcheck(L, is_alphanumeric(store_name), 1, "store name must be alphanumeric");

	lua_settop(L, 1);

	std::string store_filename  = "table_";
	store_filename             += store_name;
	store_filename             += ".json";

	std::string err;
	std::filesystem::path path = paths->get_sandbox_dir() / store_filename;
	std::string file_data      = util::load_file(path, err);

	if (file_data.empty())
	{
		lua_pushnil(L);
		return 1;
	}

	std::size_t offset = 0;

	err = util::convert_from_json(L, file_data, offset);
	if (!err.empty())
		luaL_error(L, "%s: parse error: %s at offset %d", store_filename.c_str(), err.c_str(), int(offset));

	if (lua_gettop(L) == 1)
		lua_createtable(L, 0, 0);

	return 1;
}

int DeckUtil::_lua_append_event_log(lua_State* L)
{
	util::Paths const* paths = reinterpret_cast<util::Paths const*>(lua_touserdata(L, lua_upvalueindex(1)));

	std::string_view const store_name = LuaHelpers::check_arg_string(L, 1);
	int const vtype                   = lua_type(L, 2);

	luaL_argcheck(L, is_alphanumeric(store_name), 1, "store name must be alphanumeric");
	luaL_argcheck(L, (vtype == LUA_TNUMBER || vtype == LUA_TSTRING || vtype == LUA_TBOOLEAN || vtype == LUA_TTABLE), 2, "value cannot be serialised");

	std::string json = util::convert_to_json(L, 2, false);
	if (json.empty())
		luaL_error(L, "error converting value to json");

	json += '\n';

	std::string store_filename  = "event_";
	store_filename             += store_name;
	store_filename             += ".log";

	std::string err;
	std::filesystem::path const path = paths->get_sandbox_dir() / store_filename;
	if (!util::append_to_file(path, json, err))
		luaL_error(L, "%s: error appending event log: %s", store_filename.c_str(), err.c_str());

	return 0;
}

int DeckUtil::_lua_retrieve_event_log(lua_State* L)
{
	util::Paths const* paths = reinterpret_cast<util::Paths const*>(lua_touserdata(L, lua_upvalueindex(1)));

	std::string_view const store_name = LuaHelpers::check_arg_string(L, 1);

	lua_Integer limit = 0;
	if (!lua_isnoneornil(L, 2))
		limit = LuaHelpers::check_arg_int(L, 2);

	luaL_argcheck(L, is_alphanumeric(store_name), 1, "store name must be alphanumeric");
	luaL_argcheck(L, limit >= 0, 2, "limit must be a positive integer");

	std::string store_filename  = "event_";
	store_filename             += store_name;
	store_filename             += ".log";

	std::string err;
	std::filesystem::path const path = paths->get_sandbox_dir() / store_filename;
	std::string const file_data      = util::load_file(path, err);

	std::vector<std::string_view> lines = util::split(file_data);
	while (!lines.empty() && lines.back().empty())
		lines.pop_back();

	std::size_t const last_idx  = lines.size();
	std::size_t const start_idx = (limit > 0 && std::size_t(limit) < last_idx) ? last_idx - limit : 0;
	std::size_t const count     = last_idx - start_idx;

	lua_createtable(L, count, 0);

	for (std::size_t idx = 0; idx < count; ++idx)
	{
		std::size_t offset = 0;

		err = util::convert_from_json(L, lines[start_idx + idx], offset);
		if (err.empty() && offset == 0)
			continue;

		if (!err.empty())
		{
			luaL_error(L, "%s: parse error on line %d: %s", store_filename.c_str(), start_idx + idx + 1, err.c_str());
			return 0;
		}

		lua_rawseti(L, -2, idx + 1);
	}

	return 1;
}

int DeckUtil::_lua_clipboard_text(lua_State* L)
{
	char* text = SDL_GetClipboardText();
	lua_pushstring(L, text);
	SDL_free(text);
	return 1;
}

int DeckUtil::_lua_yieldable_call(lua_State* L)
{
	luaL_checkany(L, 1);
	LuaHelpers::yieldable_call(L, lua_gettop(L) - 1);
	return 0;
}

int DeckUtil::_lua_ls(lua_State* L)
{
	util::Paths const* paths      = reinterpret_cast<util::Paths const*>(lua_touserdata(L, lua_upvalueindex(1)));
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

	bool const canon_path_contained = util::Paths::verify_path_contains_path(canon_path, sandbox);
	if (trust == LuaHelpers::Trust::Untrusted && !canon_path_contained)
		luaL_error(L, "access denied");

	bool const abs_path_contained = util::Paths::verify_path_contains_path(abs_path, sandbox);
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
			std::string fname = dir_iter->path().filename().string();
			if (!fname.starts_with('.'))
				subdirs.emplace_back(std::move(fname));
		}
		else if (std::filesystem::is_regular_file(file_status))
		{
			std::string fname = dir_iter->path().filename().string();
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

int DeckUtil::_lua_open_browser(lua_State* L)
{
	std::string_view const url_string = LuaHelpers::check_arg_string(L, 1);
	// bool const have_params            = lua_istable(L, 2);

#ifdef _WIN32
	HINSTANCE hresult = ShellExecuteA(0, 0, url_string.data(), 0, 0, SW_SHOW);
	INT_PTR result    = reinterpret_cast<INT_PTR>(hresult);
	lua_pushboolean(L, result >= 32);
#else
	bool success = true;
	if (!vfork())
	{
		execlp("xdg-open", "xdg-open", url_string.data(), nullptr);
		success = false;
		_exit(1);
	}
	lua_pushboolean(L, success);
#endif

	return 1;
}
