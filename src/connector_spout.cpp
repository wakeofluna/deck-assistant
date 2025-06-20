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

#ifdef HAVE_SPOUT

#include "connector_spout.h"
#include "deck_card.h"
#include "deck_logger.h"
#include "deck_rectangle.h"
#include "lua_helpers.h"
#include <SpoutDX.h>

char const* ConnectorSpout::LUA_TYPENAME = "deck:ConnectorSpout";

ConnectorSpout::ConnectorSpout()
    : m_spout_surface(nullptr)
    , m_spout_sender(nullptr)
    , m_card(nullptr)
    , m_wanted_width(0)
    , m_wanted_height(0)
    , m_enabled(true)
    , m_dirty(false)
    , m_need_send(false)
    , m_can_send(false)
{
	m_spout_sender = new spoutDX();
	m_spout_sender->SetSenderFormat(DXGI_FORMAT_B8G8R8A8_UNORM);
}

ConnectorSpout::~ConnectorSpout()
{
	if (m_spout_sender)
	{
		m_spout_sender->ReleaseSender();
		delete m_spout_sender;
	}

	if (m_spout_surface)
		SDL_FreeSurface(m_spout_surface);
}

void ConnectorSpout::initial_setup(lua_State* L, bool is_reload)
{
}

void ConnectorSpout::tick_inputs(lua_State* L, lua_Integer clock)
{
	if (!m_enabled)
		return;

	int wanted_width  = m_wanted_width;
	int wanted_height = m_wanted_height;
	if (m_card)
	{
		if (!wanted_width)
			wanted_width = m_card->get_surface()->w;
		if (!wanted_height)
			wanted_height = m_card->get_surface()->h;
	}

	if (!m_spout_surface || m_spout_surface->w != wanted_width || m_spout_surface->h != wanted_height)
	{
		LuaHelpers::emit_event(L, 1, "on_resize", wanted_width, wanted_height);

		if (m_spout_surface)
			SDL_FreeSurface(m_spout_surface);

		m_spout_surface = SDL_CreateRGBSurfaceWithFormat(0, wanted_width, wanted_height, 0, SDL_PIXELFORMAT_ARGB8888);

		if (!m_spout_surface)
		{
			DeckLogger::log_message(L, DeckLogger::Level::Error, "Failed to allocate surface for Spout connector: ", SDL_GetError());
			m_enabled = false;
			return;
		}

		release_spout_sender();
		m_dirty = true;
	}

	if (!m_can_send && (m_need_send || m_dirty))
		m_can_send = m_spout_sender->WaitFrameSync(m_spout_sender->GetName(), 0);
}

void ConnectorSpout::tick_outputs(lua_State* L, lua_Integer clock)
{
	if (!m_enabled)
	{
		release_spout_sender();
		return;
	}

	if (!m_spout_surface)
		return;

	if (m_dirty)
	{
		if (m_card)
			SDL_BlitScaled(m_card->get_surface(), nullptr, m_spout_surface, nullptr);
		else
			SDL_FillRect(m_spout_surface, nullptr, 0); // transparent

		m_dirty     = false;
		m_need_send = true;
	}

	if (m_need_send && m_can_send)
	{
		bool ok = m_spout_sender->SendImage(reinterpret_cast<unsigned char const*>(m_spout_surface->pixels), m_spout_surface->w, m_spout_surface->h);
		if (ok)
		{
			m_spout_sender->SetFrameSync(m_spout_sender->GetName());
			m_need_send = false;
		}
		m_can_send = false;
	}
}

void ConnectorSpout::shutdown(lua_State* L)
{
	m_enabled   = false;
	m_can_send  = false;
	m_need_send = false;
	m_spout_sender->ReleaseSender();
}

void ConnectorSpout::init_class_table(lua_State* L)
{
	Super::init_class_table(L);

	lua_pushcfunction(L, &_lua_redraw);
	lua_setfield(L, -2, "redraw");
}

void ConnectorSpout::init_instance_table(lua_State* L)
{
	{
		std::vector<char> buf;

		int num_adapters = m_spout_sender->GetNumAdapters();
		lua_createtable(L, num_adapters, 0);
		for (int i = 0; i < num_adapters; ++i)
		{
			buf.assign(256, 0);
			if (!m_spout_sender->GetAdapterName(i, buf.data(), buf.size() - 1))
				break;
			lua_pushstring(L, buf.data());
			lua_rawseti(L, -2, i + 1);
		}

		lua_setfield(L, -2, "adapters");
	}
}

int ConnectorSpout::index(lua_State* L, std::string_view const& key) const
{
	if (key == "width" || key == "pixel_width")
	{
		int wanted_width = !m_wanted_width && m_card ? m_card->get_surface()->w : m_wanted_width;
		lua_pushinteger(L, wanted_width);
	}
	else if (key == "height" || key == "pixel_height")
	{
		int wanted_height = !m_wanted_height && m_card ? m_card->get_surface()->h : m_wanted_height;
		lua_pushinteger(L, wanted_height);
	}
	else if (key == "name" || key == "sender_name")
	{
		if (m_spout_name.empty())
			const_cast<ConnectorSpout*>(this)->m_spout_name = m_spout_sender->GetName();

		lua_pushlstring(L, m_spout_name.data(), m_spout_name.size());
	}
	else if (key == "adapter")
	{
		int adapter = m_spout_sender->GetAdapter();
		lua_pushinteger(L, adapter + 1);
	}

	return lua_gettop(L) == 2 ? 0 : 1;
}

int ConnectorSpout::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "pixel_width" || key == "pixel_height" || key == "adapters")
	{
		luaL_error(L, "key %s is readonly for %s", key.data(), LUA_TYPENAME);
	}
	else if (key == "width")
	{
		int value = LuaHelpers::check_arg_int(L, 3);
		luaL_argcheck(L, (value >= 0), 3, "width must be zero or positive");
		m_wanted_width = value;
	}
	else if (key == "height")
	{
		int value = LuaHelpers::check_arg_int(L, 3);
		luaL_argcheck(L, (value >= 0), 3, "height must be zero or positive");
		m_wanted_height = value;
	}
	else if (key == "name" || key == "sender_name")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, 3);
		if (value != m_spout_name)
		{
			m_spout_name = value;
			release_spout_sender();
		}
	}
	else if (key == "adapter")
	{
		int adapter      = LuaHelpers::check_arg_int(L, 3);
		int num_adapters = m_spout_sender->GetNumAdapters();
		luaL_argcheck(L, (adapter > 0), 3, "adapter index must be positive");

		--adapter;
		int current_adapter = m_spout_sender->GetAdapter();
		if (current_adapter != adapter)
		{
			release_spout_sender();
			if (!m_spout_sender->SetAdapter(adapter))
				luaL_error(L, "Spout: failed to set adapter to index ", adapter + 1);
		}
	}
	else if (key == "card")
	{
		DeckCard* card;

		if (lua_type(L, 3) == LUA_TNIL)
			card = nullptr;
		else
			card = DeckCard::from_stack(L, 3);

		m_card  = card;
		m_dirty = true;
		LuaHelpers::newindex_store_in_instance_table(L);
	}
	else if (key.starts_with("on_"))
	{
		if (lua_type(L, 3) != LUA_TNIL)
			luaL_argcheck(L, (lua_type(L, 3) == LUA_TFUNCTION), 3, "event handlers must be functions");

		LuaHelpers::newindex_store_in_instance_table(L);
	}
	else
	{
		LuaHelpers::newindex_store_in_instance_table(L);
	}

	return 0;
}

void ConnectorSpout::release_spout_sender()
{
	m_spout_sender->ReleaseSender();
	m_spout_sender->SetSenderName(m_spout_name.c_str());
	m_can_send  = true;
	m_need_send = true;
}

int ConnectorSpout::_lua_redraw(lua_State* L)
{
	ConnectorSpout* self = from_stack(L, 1);
	// DeckRectangle* rect = nullptr;

	if (!lua_isnoneornil(L, 2))
		(void)DeckRectangle::from_stack(L, 2);

	self->m_dirty = true;
	return 0;
}

#endif
