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

#ifdef HAVE_VNC

#include "connector_vnc.h"
#include "deck_card.h"
#include "deck_logger.h"
#include "deck_rectangle_list.h"
#include "lua_helpers.h"
#include <algorithm>
#include <cassert>
#include <charconv>
#include <chrono>
#include <cstring>
#include <rfb/rfb.h>

namespace
{

enum DirtyFlags : unsigned char
{
	DirtyCard,
	DirtyMax,
};

template <std::size_t N>
inline std::string_view to_string_view(std::array<char, N> const& arr)
{
	char const* last = std::find(arr.cbegin(), arr.cend(), '\0');
	return std::string_view(arr.data(), last);
}

template <std::size_t N>
inline void assign_array(std::array<char, N>& arr, std::string_view const& value)
{
	std::size_t len = std::min(value.size(), N - 1);
	std::memcpy(arr.data(), value.data(), len);
	arr[len] = 0;
}

int set_desktop_size_hook(int width, int height, int numScreens, struct rfbExtDesktopScreen* extDesktopScreens, struct _rfbClientRec* cl)
{
	ConnectorVnc* connector = reinterpret_cast<ConnectorVnc*>(cl->screen->screenData);
	if (!connector)
		return rfbExtDesktopSize_ResizeProhibited;

	if (width < 1 || height < 1 || numScreens != 1)
		return rfbExtDesktopSize_InvalidScreenLayout;

	connector->notify_resize_request(width, height);
	return rfbExtDesktopSize_Success;
}

void ptr_event_hook(int buttonMask, int x, int y, struct _rfbClientRec* cl)
{
	rfbDefaultPtrAddEvent(buttonMask, x, y, cl);

	ConnectorVnc* connector = reinterpret_cast<ConnectorVnc*>(cl->screen->screenData);
	if (!connector)
		return;

	connector->notify_ptr_event(buttonMask, x, y);
}

} // namespace

char const* ConnectorVnc::LUA_TYPENAME = "deck:ConnectorVnc";

ConnectorVnc::ConnectorVnc()
    : m_screen_info(nullptr)
    , m_screen_surface(nullptr)
    , m_screen_width(1600)
    , m_screen_height(900)
    , m_bind_port(0)
    , m_card(nullptr)
{
	m_title.fill(0);
	m_password.fill(0);
	m_bind_address.fill(0);
	m_dirty_flags.assign(DirtyMax, false);
	m_pointer_events.reserve(8);

	assign_array(m_title, "Deck Assistant");
}

ConnectorVnc::~ConnectorVnc()
{
	if (m_screen_info)
		rfbScreenCleanup(m_screen_info);

	if (m_screen_surface)
		SDL_FreeSurface(m_screen_surface);
}

void ConnectorVnc::tick_inputs(lua_State* L, lua_Integer clock)
{
	if (!m_screen_info)
		return;

	pump_events();

	if (m_screen_surface && (m_screen_surface->w != m_screen_width || m_screen_surface->h != m_screen_height))
	{
		DeckLogger::log_message(L, DeckLogger::Level::Info, "VNC resize requested to ", m_screen_width, 'x', m_screen_height);
		LuaHelpers::emit_event(L, 1, "on_resize", m_screen_width, m_screen_height);
	}

	if (!m_pointer_events.empty())
	{
		lua_getfield(L, 1, "hotspots");
		bool const have_hotspots = DeckRectangleList::from_stack(L, -1, false);

		for (PointerState const& pointer_event : m_pointer_events)
		{
			if (pointer_event.x != m_pointer_state.x || pointer_event.y != m_pointer_state.y)
			{
				if (have_hotspots)
					DeckRectangleList::push_any_contains(L, pointer_event.x, pointer_event.y);
				else
					lua_pushnil(L);

				LuaHelpers::emit_event(L, 1, "on_mouse_motion", pointer_event.x, pointer_event.y, LuaHelpers::StackValue(L, -1));
				lua_pop(L, 1);

				m_pointer_state.x = pointer_event.x;
				m_pointer_state.y = pointer_event.y;
			}

			if (pointer_event.button_mask != m_pointer_state.button_mask)
			{
				for (unsigned int button_idx = 0; button_idx < 3; ++button_idx)
				{
					unsigned int button_mask = 1 << button_idx;
					bool const was_clicked   = m_pointer_state.button_mask & button_mask;
					bool const is_clicked    = pointer_event.button_mask & button_mask;

					if (was_clicked != is_clicked)
					{
						if (have_hotspots)
							DeckRectangleList::push_any_contains(L, pointer_event.x, pointer_event.y);
						else
							lua_pushnil(L);

						LuaHelpers::emit_event(L, 1, "on_mouse_button", m_pointer_state.x, m_pointer_state.y, button_idx + 1, is_clicked, LuaHelpers::StackValue(L, -1));
						lua_pop(L, 1);
					}
				}
				m_pointer_state.button_mask = pointer_event.button_mask;
			}
		}

		lua_pop(L, 1);
		m_pointer_events.clear();
	}
}

void ConnectorVnc::tick_outputs(lua_State* L)
{
	if (!m_screen_info)
	{
		std::array<char, 8> portbuf;

		char const* argv[12] = {
			"deck",
			"-desktop",
			m_title.data(),
			"-alwaysshared",
			"-dontdisconnect",
		};

		int argc = 5;

		if (m_password[0])
		{
			argv[argc++] = "-passwd";
			argv[argc++] = m_password.data();
		}

		if (m_bind_address[0])
		{
			argv[argc++] = "-listen";
			argv[argc++] = m_bind_address.data();
		}

		if (m_bind_port)
		{
			portbuf.fill(0);
			std::to_chars(portbuf.begin(), portbuf.end(), m_bind_port, 10);

			argv[argc++] = "-rfbport";
			argv[argc++] = portbuf.data();
		}

		argv[argc] = nullptr;

		m_screen_info = rfbGetScreen(&argc, (char**)&argv, m_screen_width, m_screen_height, 8, 3, 4);
		if (!m_screen_info)
			return;

		m_screen_info->screenData         = this;
		m_screen_info->setDesktopSizeHook = &set_desktop_size_hook;
		m_screen_info->ptrAddEvent        = &ptr_event_hook;

		std::fill(m_dirty_flags.begin(), m_dirty_flags.end(), true);
	}

	if (!m_screen_surface || m_screen_surface->w != m_screen_width || m_screen_surface->h != m_screen_height)
	{
		bool const had_surface     = m_screen_surface;
		bool const had_framebuffer = m_screen_info->frameBuffer;

		if (m_screen_surface)
			SDL_FreeSurface(m_screen_surface);

		m_screen_surface = SDL_CreateRGBSurfaceWithFormat(0, m_screen_width, m_screen_height, 0, SDL_PIXELFORMAT_XBGR8888);
		if (!m_screen_surface)
			return;

		if (!had_surface)
			LuaHelpers::emit_event(L, 1, "on_resize", m_screen_width, m_screen_height);

		m_dirty_flags[DirtyCard] = false;
		if (m_card)
		{
			SDL_Surface* surface = m_card->get_surface();
			SDL_BlitScaled(surface, nullptr, m_screen_surface, nullptr);
		}

		rfbNewFramebuffer(m_screen_info, (char*)m_screen_surface->pixels, m_screen_width, m_screen_height, 8, 3, 4);

		if (!had_framebuffer)
			rfbInitServer(m_screen_info);
	}

	if (m_dirty_flags[DirtyCard] && m_card)
	{
		m_dirty_flags[DirtyCard] = false;

		SDL_Surface* surface = m_card->get_surface();
		SDL_BlitScaled(surface, nullptr, m_screen_surface, nullptr);

		rfbMarkRectAsModified(m_screen_info, 0, 0, m_screen_width, m_screen_height);
	}
}

void ConnectorVnc::shutdown(lua_State* L)
{
	close_vnc();
}

void ConnectorVnc::init_class_table(lua_State* L)
{
	Super::init_class_table(L);

	lua_pushcfunction(L, &_lua_redraw);
	lua_setfield(L, -2, "redraw");
}

void ConnectorVnc::init_instance_table(lua_State* L)
{
	DeckRectangleList::push_new(L);
	lua_setfield(L, -2, "hotspots");
}

int ConnectorVnc::index(lua_State* L, std::string_view const& key) const
{
	if (key == "connected")
	{
		lua_pushboolean(L, m_screen_info ? 1 : 0);
	}
	else if (key == "interface")
	{
		lua_pushlstring(L, m_bind_address.data(), m_bind_address.size());
	}
	else if (key == "port")
	{
		lua_pushinteger(L, m_bind_port);
	}
	else if (key == "title")
	{
		lua_pushlstring(L, m_title.data(), m_title.size());
	}
	else if (key == "password")
	{
		lua_pushlstring(L, m_password.data(), m_password.size());
	}
	else if (key == "width" || key == "pixel_width")
	{
		lua_pushinteger(L, m_screen_width);
	}
	else if (key == "height" || key == "pixel_height")
	{
		lua_pushinteger(L, m_screen_height);
	}

	return lua_gettop(L) == 2 ? 0 : 1;
}

int ConnectorVnc::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "connected" || key == "pixel_width" || key == "pixel_height")
	{
		luaL_error(L, "key %s is readonly for %s", key.data(), LUA_TYPENAME);
	}
	else if (key == "interface")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, 3, true);
		if (to_string_view(m_bind_address) != value)
		{
			close_vnc();
			assign_array(m_bind_address, value);
		}
	}
	else if (key == "port")
	{
		int value = LuaHelpers::check_arg_int(L, 3);
		luaL_argcheck(L, (value >= 0 && value < 65536), 3, "port must be between 0 and 65535");

		if (m_bind_port != value)
		{
			close_vnc();
			m_bind_port = value;
		}
	}
	else if (key == "title")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, 3);
		assign_array(m_title, value);
	}
	else if (key == "password")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, 3);
		assign_array(m_password, value);
	}
	else if (key == "width")
	{
		int value = LuaHelpers::check_arg_int(L, 3);
		luaL_argcheck(L, (value > 0), 3, "width must be positive");
		m_screen_width = value;
	}
	else if (key == "height")
	{
		int value = LuaHelpers::check_arg_int(L, 3);
		luaL_argcheck(L, (value > 0), 3, "height must be positive");
		m_screen_height = value;
	}
	else if (key == "card")
	{
		DeckCard* card;

		if (lua_type(L, 3) == LUA_TNIL)
			card = nullptr;
		else
			card = DeckCard::from_stack(L, 3);

		if (card)
		{
			SDL_Surface* surface = card->get_surface();
			if (surface->format->format != SDL_PIXELFORMAT_ARGB8888)
				luaL_error(L, "card does not have correct surface format, use a new deck:Card instead");
		}

		m_card                   = card;
		m_dirty_flags[DirtyCard] = true;
		LuaHelpers::newindex_store_in_instance_table(L);
	}
	else if (key == "hotspots")
	{
		if (lua_type(L, 3) != LUA_TNIL)
			DeckRectangleList::from_stack(L, 3);

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

void ConnectorVnc::notify_resize_request(int new_width, int new_height)
{
	m_screen_width  = new_width;
	m_screen_height = new_height;
}

void ConnectorVnc::notify_ptr_event(int button_mask, int x, int y)
{
	m_pointer_events.emplace_back(PointerState { button_mask, x, y });
}

void ConnectorVnc::pump_events()
{
	assert(m_screen_info);
	while (rfbProcessEvents(m_screen_info, 0))
	{
	}
}

void ConnectorVnc::close_vnc()
{
	if (m_screen_info)
	{
		rfbShutdownServer(m_screen_info, true);
		pump_events();

		rfbScreenCleanup(m_screen_info);
		m_screen_info = nullptr;
	}

	if (m_screen_surface)
	{
		SDL_FreeSurface(m_screen_surface);
		m_screen_surface = nullptr;
	}
}

int ConnectorVnc::_lua_redraw(lua_State* L)
{
	ConnectorVnc* self             = from_stack(L, 1);
	self->m_dirty_flags[DirtyCard] = true;
	return 0;
}

#endif
