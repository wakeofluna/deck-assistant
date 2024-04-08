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

void DeckPromise::mark_as_fulfilled()
{
	if (m_time_fulfilled == NotFulfilled || m_time_fulfilled == IsTimedOut)
		m_time_fulfilled = IsFulfilled;
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
