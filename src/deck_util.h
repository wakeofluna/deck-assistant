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

#ifndef DECK_ASSISTANT_DECK_UTIL_H
#define DECK_ASSISTANT_DECK_UTIL_H

#include "lua_class.h"
#include "lua_helpers.h"

namespace util
{
class Paths;
}

class DeckUtil : public LuaClass<DeckUtil>
{
public:
	DeckUtil(LuaHelpers::Trust trust, util::Paths const& paths);

	static char const* LUA_TYPENAME;

	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int newindex(lua_State* L);

private:
	static int _lua_from_base64(lua_State* L);
	static int _lua_to_base64(lua_State* L);
	static int _lua_from_hex(lua_State* L);
	static int _lua_to_hex(lua_State* L);
	static int _lua_from_json(lua_State* L);
	static int _lua_to_json(lua_State* L);
	static int _lua_sha1(lua_State* L);
	static int _lua_sha256(lua_State* L);
	static int _lua_random_bytes(lua_State* L);
	static int _lua_store_secret(lua_State* L);
	static int _lua_retrieve_secret(lua_State* L);
	static int _lua_store_table(lua_State* L);
	static int _lua_retrieve_table(lua_State* L);
	static int _lua_append_event_log(lua_State* L);
	static int _lua_retrieve_event_log(lua_State* L);
	static int _lua_ls(lua_State* L);

	util::Paths const& m_paths;
	LuaHelpers::Trust m_trust;
};

#endif // DECK_ASSISTANT_DECK_UTIL_H
