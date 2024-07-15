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

#include "deck_promise.h"
#include "deck_module.h"
#include "lua_helpers.h"

namespace
{

constexpr lua_Integer const NotPromised  = -1;
constexpr lua_Integer const NotFulfilled = -2;
constexpr lua_Integer const IsFulfilled  = -3;
constexpr lua_Integer const IsTimedOut   = -4;

} // namespace

char const* DeckPromise::LUA_TYPENAME = "deck:Promise";

DeckPromise::DeckPromise(int timeout) noexcept
    : m_time_promised(NotPromised)
    , m_time_fulfilled(NotFulfilled)
    , m_timeout(timeout)
{
}

bool DeckPromise::check_wakeup(lua_Integer clock)
{
	if (m_time_fulfilled == NotFulfilled)
	{
		if (clock >= m_time_promised + m_timeout)
		{
			m_time_fulfilled = IsTimedOut;
			return true;
		}
		return false;
	}

	if (m_time_fulfilled == IsFulfilled)
		m_time_fulfilled = clock;

	return true;
}

bool DeckPromise::mark_as_fulfilled()
{
	if (m_time_fulfilled == NotFulfilled || m_time_fulfilled == IsTimedOut)
	{
		m_time_fulfilled = IsFulfilled;
		return true;
	}
	else
	{
		return false;
	}
}

void DeckPromise::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &_lua_fulfill);
	lua_setfield(L, -2, "fulfill");

	lua_pushcfunction(L, &_lua_reset);
	lua_setfield(L, -2, "reset");

	lua_pushcfunction(L, &_lua_wait);
	lua_setfield(L, -2, "wait");
}

void DeckPromise::init_instance_table(lua_State* L)
{
	m_time_promised = DeckModule::get_clock(L);
}

int DeckPromise::index(lua_State* L, std::string_view const& key) const
{
	if (key == "time_promised")
	{
		if (m_time_promised != NotPromised)
			lua_pushinteger(L, m_time_promised);
	}
	else if (key == "time_fulfilled")
	{
		if (m_time_fulfilled >= m_time_promised)
			lua_pushinteger(L, m_time_fulfilled);
	}
	else if (key == "time" || key == "time_used" || key == "time_taken")
	{
		if (m_time_fulfilled >= m_time_promised)
			lua_pushinteger(L, m_time_fulfilled - m_time_promised);
	}
	else if (key == "ready")
	{
		lua_pushboolean(L, m_time_fulfilled >= m_time_promised);
	}
	else if (key == "timeout")
	{
		lua_pushinteger(L, m_timeout);
	}
	else if (key == "timed_out")
	{
		lua_pushboolean(L, m_time_fulfilled == IsTimedOut);
	}
	return lua_gettop(L) == 2 ? 0 : 1;
}

int DeckPromise::newindex(lua_State* L)
{
	luaL_error(L, "%s instance is closed for modifications", LUA_TYPENAME);
	return 0;
}

int DeckPromise::tostring(lua_State* L) const
{
	if (m_time_fulfilled >= m_time_promised)
		lua_pushfstring(L, "%s { ready=true, time_promised=%d, time_used=%d }", LUA_TYPENAME, m_time_promised, m_time_fulfilled - m_time_promised);
	else if (m_time_fulfilled == IsTimedOut)
		lua_pushfstring(L, "%s { ready=false, time_promised=%d, timeout=%d, timed_out }", LUA_TYPENAME, m_time_promised, m_timeout);
	else
		lua_pushfstring(L, "%s { ready=false, time_promised=%d, timeout=%d }", LUA_TYPENAME, m_time_promised, m_timeout);

	return 1;
}

int DeckPromise::_lua_fulfill(lua_State* L)
{
	DeckPromise* self = from_stack(L, 1);
	luaL_checkany(L, 2);

	if (self->mark_as_fulfilled())
	{
		LuaHelpers::push_instance_table(L, 1);
		lua_pushliteral(L, "value");
		lua_pushvalue(L, 2);
		lua_rawset(L, -3);
	}

	return 0;
}

int DeckPromise::_lua_reset(lua_State* L)
{
	DeckPromise* self      = from_stack(L, 1);
	self->m_time_promised  = DeckModule::get_clock(L);
	self->m_time_fulfilled = NotFulfilled;
	return 0;
}

int DeckPromise::_lua_wait(lua_State* L)
{
	DeckPromise* self       = from_stack(L, 1);
	lua_Integer new_timeout = self->m_timeout;

	if (!lua_isnone(L, 2))
	{
		new_timeout = LuaHelpers::check_arg_int(L, 2);
		luaL_argcheck(L, (new_timeout >= 0), 2, "timeout must be zero or positive (in msec)");
	}

	if (self->m_time_promised == NotPromised)
		return 0;

	if (self->m_time_fulfilled == IsTimedOut)
		self->m_time_fulfilled = NotFulfilled;

	if (self->m_time_fulfilled == NotFulfilled)
	{
		self->m_timeout = new_timeout;
		lua_settop(L, 1);
		return lua_yield(L, 1);
	}

	LuaHelpers::push_instance_table(L, 1);
	lua_pushliteral(L, "value");
	lua_rawget(L, -2);

	return 1;
}
