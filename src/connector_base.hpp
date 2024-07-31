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

#ifndef DECK_ASSISTANT_CONNECTOR_BASE_HPP
#define DECK_ASSISTANT_CONNECTOR_BASE_HPP

#include "connector_base.h"
#include "deck_logger.h"
#include "lua_helpers.h"

template <typename T>
void ConnectorBase<T>::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &_lua_tick_inputs);
	lua_setfield(L, -2, "tick_inputs");

	lua_pushcfunction(L, &_lua_tick_outputs);
	lua_setfield(L, -2, "tick_outputs");

	lua_pushcfunction(L, &_lua_shutdown);
	lua_setfield(L, -2, "shutdown");
}

template <typename T>
void ConnectorBase<T>::finalize(lua_State* L)
{
	shutdown(L);
	DeckLogger::log_message(L, DeckLogger::Level::Trace, T::LUA_TYPENAME, " finalized");
}

template <typename T>
int ConnectorBase<T>::_lua_tick_inputs(lua_State* L)
{
	T* self           = LuaClass<T>::from_stack(L, 1);
	lua_Integer clock = LuaHelpers::check_arg_int(L, 2);

	self->tick_inputs(L, clock);
	return 0;
}

template <typename T>
int ConnectorBase<T>::_lua_tick_outputs(lua_State* L)
{
	T* self           = LuaClass<T>::from_stack(L, 1);
	lua_Integer clock = LuaHelpers::check_arg_int(L, 2);

	self->tick_outputs(L, clock);
	return 0;
}

template <typename T>
int ConnectorBase<T>::_lua_shutdown(lua_State* L)
{
	T* self = LuaClass<T>::from_stack(L, 1);
	self->shutdown(L);
	return 0;
}

#endif // DECK_ASSISTANT_CONNECTOR_BASE_HPP
