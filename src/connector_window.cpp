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

#include "connector_window.h"
#include "deck_card.h"
#include "deck_logger.h"
#include "deck_module.h"
#include "deck_rectangle.h"
#include "lua_helpers.h"

char const* ConnectorWindow::LUA_TYPENAME = "deck:ConnectorWindow";

namespace
{

void adjust_coordinates_for_scale(SDL_Window const* window, DeckCard const* card, int& x, int& y)
{
	if (window && card)
	{
		int w1, h1;
		SDL_GetWindowSize(const_cast<SDL_Window*>(window), &w1, &h1);

		if (int w2 = card->get_surface()->w; w1 != w2)
			x = x * w2 / w1;

		if (int h2 = card->get_surface()->h; h1 != h2)
			y = y * h2 / h1;
	}
}

void dispatch_key_event(lua_State* L, SDL_KeyboardEvent const& event, char const* name)
{
	lua_getfield(L, 1, name);
	if (lua_type(L, -1) == LUA_TFUNCTION)
	{
		lua_pushvalue(L, 1);
		lua_pushinteger(L, event.keysym.mod);
		lua_pushinteger(L, event.keysym.sym);
		lua_pushinteger(L, event.keysym.scancode);
		LuaHelpers::yieldable_call(L, 4);
	}
	else
	{
		lua_pop(L, 1);
	}
}

} // namespace

ConnectorWindow::ConnectorWindow()
    : m_window(nullptr)
    , m_wanted_width(1600)
    , m_wanted_height(900)
    , m_wanted_visible(true)
    , m_exit_on_close(true)
    , m_event_size_changed(false)
    , m_event_surface_dirty(false)
    , m_card(nullptr)
{
}

ConnectorWindow::~ConnectorWindow()
{
	if (m_window)
		SDL_DestroyWindow(m_window);
}

void ConnectorWindow::initial_setup(lua_State* L, bool is_reload)
{
	if (m_window)
		m_event_size_changed = true;
}

void ConnectorWindow::tick_inputs(lua_State* L, lua_Integer clock)
{
	std::lock_guard guard(m_mutex);

	if (!m_window && !attempt_create_window(L))
		return;

	for (SDL_Event const& event : m_pending_events)
	{
		switch (event.type)
		{
			case SDL_WINDOWEVENT:
				handle_window_event(L, event.window);
				break;
			case SDL_MOUSEMOTION:
				handle_motion_event(L, event.motion);
				break;
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
				handle_button_event(L, event.button);
				break;
			case SDL_MOUSEWHEEL:
				handle_wheel_event(L, event.wheel);
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				handle_keyboard_event(L, event.key);
				break;
			case SDL_TEXTINPUT:
				handle_text_input_event(L, event.text);
				break;
			case SDL_TEXTEDITING:
				break;
			case SDL_TEXTEDITING_EXT:
				SDL_free(event.editExt.text);
				break;
		}
	}

	m_pending_events.clear();

	if (m_event_size_changed)
	{
		m_event_size_changed  = false;
		m_event_surface_dirty = true;

		int width;
		int height;
		SDL_GetWindowSize(m_window, &width, &height);

		LuaHelpers::emit_event(L, 1, "on_resize", width, height);
	}
}

void ConnectorWindow::tick_outputs(lua_State* L, lua_Integer clock)
{
	if (!m_window)
		return;

	if (m_wanted_title.has_value())
	{
		SDL_SetWindowTitle(m_window, m_wanted_title->c_str());
		m_wanted_title.reset();
	}

	if (m_wanted_width.has_value() || m_wanted_height.has_value())
	{
		int width;
		int height;
		SDL_GetWindowSize(m_window, &width, &height);

		int new_width  = m_wanted_width.value_or(width);
		int new_height = m_wanted_height.value_or(height);
		if (new_width != width || new_height != height)
			SDL_SetWindowSize(m_window, new_width, new_height);

		m_wanted_width.reset();
		m_wanted_height.reset();
	}

	if (m_wanted_visible.has_value())
	{
		if (m_wanted_visible.value())
			SDL_ShowWindow(m_window);
		else
			SDL_HideWindow(m_window);

		m_wanted_visible.reset();
	}

	if (m_event_surface_dirty)
	{
		m_event_surface_dirty = false;

		SDL_Surface* surface      = SDL_GetWindowSurface(m_window);
		SDL_Surface* card_surface = m_card ? m_card->get_surface() : nullptr;

		if (card_surface)
			SDL_BlitScaled(card_surface, nullptr, surface, nullptr);
		else
			SDL_FillRect(surface, nullptr, SDL_MapRGB(surface->format, 0, 0, 0));

		SDL_UpdateWindowSurface(m_window);
	}
}

void ConnectorWindow::shutdown(lua_State* L)
{
	if (m_window)
	{
		SDL_DelEventWatch(&_sdl_event_filter, this);
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
	}
}

void ConnectorWindow::init_class_table(lua_State* L)
{
	Super::init_class_table(L);

	lua_pushcfunction(L, &_lua_redraw);
	lua_setfield(L, -2, "redraw");
}

void ConnectorWindow::init_instance_table(lua_State* L)
{
	LuaHelpers::create_callback_warning(L, "on_mouse_button");
	LuaHelpers::create_callback_warning(L, "on_mouse_motion");
	LuaHelpers::create_callback_warning(L, "on_mouse_scroll");
	LuaHelpers::create_callback_warning(L, "on_key_down");
	LuaHelpers::create_callback_warning(L, "on_key_press");
	LuaHelpers::create_callback_warning(L, "on_key_up");
	LuaHelpers::create_callback_warning(L, "on_text_input");
	LuaHelpers::create_callback_warning(L, "on_resize");
}

int ConnectorWindow::index(lua_State* L, std::string_view const& key) const
{
	if (key == "connected")
	{
		lua_pushboolean(L, m_window ? 1 : 0);
	}
	else if (key == "title")
	{
		if (m_wanted_title.has_value())
		{
			lua_pushlstring(L, m_wanted_title->data(), m_wanted_title->size());
		}
		else if (m_window)
		{
			lua_pushstring(L, SDL_GetWindowTitle(m_window));
		}
	}
	else if (key == "w" || key == "width")
	{
		if (m_wanted_width.has_value())
		{
			lua_pushinteger(L, m_wanted_width.value());
		}
		else if (m_window)
		{
			int width;
			int height;
			SDL_GetWindowSize(m_window, &width, &height);
			lua_pushinteger(L, width);
		}
	}
	else if (key == "h" || key == "height")
	{
		if (m_wanted_height.has_value())
		{
			lua_pushinteger(L, m_wanted_height.value());
		}
		else if (m_window)
		{
			int width;
			int height;
			SDL_GetWindowSize(m_window, &width, &height);
			lua_pushinteger(L, height);
		}
	}
	else if (key == "pixel_width")
	{
		if (m_window)
		{
			int width;
			int height;
			SDL_GetWindowSizeInPixels(m_window, &width, &height);
			lua_pushinteger(L, width);
		}
	}
	else if (key == "pixel_height")
	{
		if (m_window)
		{
			int width;
			int height;
			SDL_GetWindowSizeInPixels(m_window, &width, &height);
			lua_pushinteger(L, height);
		}
	}
	else if (key == "visible")
	{
		if (m_wanted_visible.has_value())
		{
			lua_pushboolean(L, m_wanted_visible.value());
		}
		else if (m_window)
		{
			lua_pushboolean(L, SDL_GetWindowFlags(m_window) & SDL_WINDOW_SHOWN);
		}
	}
	else if (key == "exit_on_close" || key == "quit_on_close")
	{
		lua_pushboolean(L, m_exit_on_close);
	}

	return lua_gettop(L) == 2 ? 0 : 1;
}

int ConnectorWindow::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "connected" || key == "pixel_width" || key == "pixel_height")
	{
		luaL_error(L, "key %s is readonly for %s", key.data(), LUA_TYPENAME);
	}
	else if (key == "title")
	{
		std::string_view title = LuaHelpers::check_arg_string(L, 3);
		m_wanted_title         = title;
	}
	else if (key == "w" || key == "width")
	{
		int width = LuaHelpers::check_arg_int(L, 3);
		luaL_argcheck(L, (width > 0), 3, "width must be larger than zero");
		m_wanted_width = width;
	}
	else if (key == "h" || key == "height")
	{
		int height = LuaHelpers::check_arg_int(L, 3);
		luaL_argcheck(L, (height > 0), 3, "height must be larger than zero");
		m_wanted_height = height;
	}
	else if (key == "visible")
	{
		luaL_argcheck(L, (lua_type(L, 3) == LUA_TBOOLEAN), 3, "visible must be a boolean");
		m_wanted_visible = lua_toboolean(L, 3);
	}
	else if (key == "exit_on_close" || key == "quit_on_close")
	{
		luaL_argcheck(L, (lua_type(L, 3) == LUA_TBOOLEAN), 3, "exit_on_close must be a boolean");
		m_exit_on_close = lua_toboolean(L, 3);
	}
	else if (key == "card")
	{
		DeckCard* card;

		if (lua_type(L, 3) == LUA_TNIL)
			card = nullptr;
		else
			card = DeckCard::from_stack(L, 3);

		m_card                = card;
		m_event_surface_dirty = true;
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

bool ConnectorWindow::attempt_create_window(lua_State* L)
{
	if (!m_window)
	{
		char const* title = m_wanted_title.has_value() ? m_wanted_title->c_str() : "Deck Assistant";
		int width         = m_wanted_width.value_or(1600);
		int height        = m_wanted_height.value_or(900);
		bool visible      = m_wanted_visible.value_or(true);

		Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
		if (!visible)
			flags |= SDL_WINDOW_HIDDEN;

		m_window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
		m_wanted_title.reset();
		m_wanted_width.reset();
		m_wanted_height.reset();
		m_wanted_visible.reset();

		if (!m_window)
		{
			DeckLogger::log_message(L, DeckLogger::Level::Error, "failed to create window: ", SDL_GetError());
		}
		else
		{
			SDL_AddEventWatch(&_sdl_event_filter, this);
			m_event_size_changed  = true;
			m_event_surface_dirty = true;
		}
	}

	return m_window;
}

int ConnectorWindow::_lua_redraw(lua_State* L)
{
	ConnectorWindow* self = from_stack(L, 1);
	// DeckRectangle* rect = nullptr;

	if (!lua_isnoneornil(L, 2))
		/*rect =*/DeckRectangle::from_stack(L, 2);

	std::lock_guard guard(self->m_mutex);
	self->m_event_surface_dirty = true;

	return 0;
}

int ConnectorWindow::_sdl_event_filter(void* userdata, SDL_Event* event)
{
	// This may run out of context, so we must catch and queue these events locally using atomics

	ConnectorWindow* self  = reinterpret_cast<ConnectorWindow*>(userdata);
	Uint32 const window_id = self->m_window ? SDL_GetWindowID(self->m_window) : -1;

	if (event->type == SDL_MOUSEMOTION && event->motion.windowID == window_id)
	{
		// DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window mouse motion at ", event->motion.x, ',', event->motion.y);
		std::lock_guard guard(self->m_mutex);
		self->m_pending_events.push_back(*event);
	}
	else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.windowID == window_id)
	{
		DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window mouse button ", int(event->button.button), " down at ", event->button.x, ',', event->button.y);
		std::lock_guard guard(self->m_mutex);
		self->m_pending_events.push_back(*event);
	}
	else if (event->type == SDL_MOUSEBUTTONUP && event->button.windowID == window_id)
	{
		DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window mouse button ", int(event->button.button), " up at ", event->button.x, ',', event->button.y);
		std::lock_guard guard(self->m_mutex);
		self->m_pending_events.push_back(*event);
	}
	else if (event->type == SDL_MOUSEWHEEL && event->wheel.windowID == window_id)
	{
		DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window mouse wheel ", event->wheel.x, ',', -event->wheel.y, " at ", event->wheel.x, ',', event->wheel.y);
		std::lock_guard guard(self->m_mutex);
		self->m_pending_events.push_back(*event);
	}
	else if (event->type == SDL_WINDOWEVENT && event->window.windowID == window_id)
	{
		std::lock_guard guard(self->m_mutex);
		self->m_pending_events.push_back(*event);
	}
	else if (event->type == SDL_TEXTINPUT && event->text.windowID == window_id)
	{
		std::lock_guard guard(self->m_mutex);
		self->m_pending_events.push_back(*event);
	}
	else if (event->type == SDL_TEXTEDITING && event->edit.windowID == window_id)
	{
		std::lock_guard guard(self->m_mutex);
		self->m_pending_events.push_back(*event);
	}
	else if (event->type == SDL_TEXTEDITING_EXT && event->editExt.windowID == window_id)
	{
		std::lock_guard guard(self->m_mutex);
		self->m_pending_events.push_back(*event);
	}
	else if ((event->type == SDL_KEYUP || event->type == SDL_KEYDOWN) && event->key.windowID == window_id)
	{
		std::lock_guard guard(self->m_mutex);
		self->m_pending_events.push_back(*event);
	}

	return 0;
}

void ConnectorWindow::handle_window_event(lua_State* L, SDL_WindowEvent const& event)
{
	switch (event.event)
	{
		case SDL_WINDOWEVENT_EXPOSED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window redraw requested");
			m_event_surface_dirty = true;
			break;
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window changed size to ", event.data1, 'x', event.data2);
			m_event_surface_dirty = true;
			m_event_size_changed  = true;
			break;
		case SDL_WINDOWEVENT_SHOWN:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window became shown");
			m_event_surface_dirty = true;
			break;
		case SDL_WINDOWEVENT_HIDDEN:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window became hidden");
			break;
		case SDL_WINDOWEVENT_MOVED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window moved to ", event.data1, 'x', event.data2);
			break;
		case SDL_WINDOWEVENT_RESIZED:
			// DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window resized to ", event.data1, 'x', event.data2);
			break;
		case SDL_WINDOWEVENT_MINIMIZED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window became minimized");
			break;
		case SDL_WINDOWEVENT_MAXIMIZED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window became maximized");
			break;
		case SDL_WINDOWEVENT_RESTORED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window became restored");
			m_event_surface_dirty = true;
			break;
		case SDL_WINDOWEVENT_ENTER:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window received pointer focus");
			break;
		case SDL_WINDOWEVENT_LEAVE:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window lost pointer focus");
			break;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window became focused");
			{
				DeckModule* deck = DeckModule::push_global_instance(L);
				deck->set_reload_requested();
				lua_pop(L, 1);
			}
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window became unfocused");
			break;
		case SDL_WINDOWEVENT_CLOSE:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window got request to close");
			if (m_exit_on_close)
			{
				DeckModule* deck = DeckModule::push_global_instance(L);
				deck->set_exit_requested(0);
				lua_pop(L, 1);
			}
			else
			{
				m_wanted_visible = false;
			}
			break;
		case SDL_WINDOWEVENT_TAKE_FOCUS:
		case SDL_WINDOWEVENT_HIT_TEST:
		case SDL_WINDOWEVENT_ICCPROF_CHANGED:
		case SDL_WINDOWEVENT_DISPLAY_CHANGED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window event with type ", event.event);
			break;
		default:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Trace, "Window UNKNOWN event with type ", event.event);
			break;
	}
}

void ConnectorWindow::handle_motion_event(lua_State* L, SDL_MouseMotionEvent const& event)
{
	lua_getfield(L, 1, "on_mouse_motion");
	if (lua_type(L, -1) == LUA_TFUNCTION)
	{
		int pointer_x = event.x;
		int pointer_y = event.y;
		adjust_coordinates_for_scale(m_window, m_card, pointer_x, pointer_y);

		lua_pushvalue(L, 1);
		lua_pushinteger(L, pointer_x);
		lua_pushinteger(L, pointer_y);
		LuaHelpers::yieldable_call(L, 3);
	}
	else
	{
		lua_pop(L, 1);
	}
}

void ConnectorWindow::handle_button_event(lua_State* L, SDL_MouseButtonEvent const& event)
{
	lua_getfield(L, 1, "on_mouse_button");
	if (lua_type(L, -1) == LUA_TFUNCTION)
	{
		int pointer_x = event.x;
		int pointer_y = event.y;
		adjust_coordinates_for_scale(m_window, m_card, pointer_x, pointer_y);

		lua_pushvalue(L, 1);
		lua_pushinteger(L, pointer_x);
		lua_pushinteger(L, pointer_y);
		lua_pushinteger(L, event.button);
		lua_pushboolean(L, event.type == SDL_MOUSEBUTTONDOWN);
		LuaHelpers::yieldable_call(L, 5);
	}
	else
	{
		lua_pop(L, 1);
	}
}

void ConnectorWindow::handle_wheel_event(lua_State* L, SDL_MouseWheelEvent const& event)
{
	lua_getfield(L, 1, "on_mouse_scroll");
	if (lua_type(L, -1) == LUA_TFUNCTION)
	{
		lua_pushvalue(L, 1);
		lua_pushinteger(L, event.mouseX);
		lua_pushinteger(L, event.mouseY);

		if (event.direction == SDL_MOUSEWHEEL_NORMAL)
		{
			lua_pushnumber(L, event.preciseX);
			lua_pushnumber(L, -event.preciseY);
		}
		else
		{
			lua_pushnumber(L, -event.preciseX);
			lua_pushnumber(L, event.preciseY);
		}

		LuaHelpers::yieldable_call(L, 5);
	}
	else
	{
		lua_pop(L, 1);
	}
}

void ConnectorWindow::handle_keyboard_event(lua_State* L, SDL_KeyboardEvent const& event)
{
	if (event.type == SDL_KEYUP)
	{
		dispatch_key_event(L, event, "on_key_up");
	}
	else if (event.type == SDL_KEYDOWN)
	{
		if (!event.repeat)
			dispatch_key_event(L, event, "on_key_down");

		dispatch_key_event(L, event, "on_key_press");
	}
}

void ConnectorWindow::handle_text_input_event(lua_State* L, SDL_TextInputEvent const& event)
{
	lua_getfield(L, 1, "on_text_input");
	if (lua_type(L, -1) == LUA_TFUNCTION)
	{
		lua_pushvalue(L, 1);
		lua_pushstring(L, event.text);
		LuaHelpers::yieldable_call(L, 2);
	}
	else
	{
		lua_pop(L, 1);
	}
}
