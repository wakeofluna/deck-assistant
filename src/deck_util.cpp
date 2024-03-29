#include "deck_util.h"
#include "lua_helpers.h"
#include "util_blob.h"

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

int DeckUtil::_lua_sha1(lua_State* L)
{
	std::string_view input = LuaHelpers::to_string_view(L, 1);

#ifdef HAVE_GNUTLS
	BlobView blob = input;
	Blob output   = blob.sha1();

	lua_pushlstring(L, (char const*)output.data(), output.size());
	return 1;
#else
	luaL_error(L, "function not implemented due to lack of gnutls");
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
	luaL_error(L, "function not implemented due to lack of gnutls");
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
