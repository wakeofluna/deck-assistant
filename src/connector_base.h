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

#ifndef DECK_ASSISTANT_CONNECTOR_BASE_H
#define DECK_ASSISTANT_CONNECTOR_BASE_H

#include "lua_class.h"

template <typename T>
class ConnectorBase : public LuaClass<T>
{
public:
	using Super = ConnectorBase<T>;

	virtual void initial_setup(lua_State* L, bool is_reload);
	virtual void tick_inputs(lua_State* L, lua_Integer clock)  = 0;
	virtual void tick_outputs(lua_State* L, lua_Integer clock) = 0;
	virtual void shutdown(lua_State* L)                        = 0;

	static bool const LUA_ENABLE_PUSH_THIS = true;
	static void init_class_table(lua_State* L);
	virtual void finalize(lua_State* L);

protected:
	static int _lua_initial_setup(lua_State* L);
	static int _lua_tick_inputs(lua_State* L);
	static int _lua_tick_outputs(lua_State* L);
	static int _lua_shutdown(lua_State* L);
};

#endif
