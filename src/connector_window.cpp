#include "connector_window.h"
#include "deck_card.h"
#include "deck_logger.h"
#include "deck_rectangle_list.h"
#include "lua_helpers.h"

template class ConnectorBase<ConnectorWindow>;

char const* ConnectorWindow::LUA_TYPENAME = "deck:ConnectorWindow";

ConnectorWindow::ConnectorWindow()
    : m_window(nullptr)
    , m_wanted_width(1600)
    , m_wanted_height(900)
    , m_wanted_visible(true)
    , m_event_size_changed(false)
    , m_event_surface_dirty(false)
    , m_card(nullptr)
    , m_hotspots(nullptr)
{
}

ConnectorWindow::~ConnectorWindow()
{
	if (m_window)
		SDL_DestroyWindow(m_window);
}

void ConnectorWindow::tick_inputs(lua_State* L, lua_Integer clock)
{
	std::lock_guard guard(m_mutex);

	if (!m_window && !attempt_create_window(L))
		return;

	if (m_event_size_changed)
	{
		m_event_size_changed  = false;
		m_event_surface_dirty = true;
		emit_event(L, "on_resize");
	}

	for (SDL_Event const& event : m_pending_events)
	{
		switch (event.type)
		{
			case SDL_WINDOWEVENT:
				handle_window_event(L, event);
				break;
			case SDL_MOUSEMOTION:
				handle_motion_event(L, event);
				break;
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
				handle_button_event(L, event);
				break;
		}
	}

	m_pending_events.clear();
}

void ConnectorWindow::tick_outputs(lua_State* L)
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
}

void ConnectorWindow::init_instance_table(lua_State* L)
{
	m_hotspots = DeckRectangleList::push_new(L);
	lua_setfield(L, -2, "hotspots");
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

	return lua_gettop(L) == 2 ? 0 : 1;
}

int ConnectorWindow::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "connected" || key == "pixel_width" || key == "pixel_height")
	{
		luaL_error(L, "key %s is readonly for %s", key.data(), LUA_TYPENAME);
		return 0;
	}

	if (key == "title")
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
	else if (key == "card")
	{
		DeckCard* card;

		if (lua_type(L, 3) == LUA_TNIL)
			card = nullptr;
		else
			card = DeckCard::from_stack(L, 3);

		m_card = card;
		LuaHelpers::newindex_store_in_instance_table(L);
	}
	else if (key == "hotspots")
	{
		DeckRectangleList* rect_list;

		if (lua_type(L, 3) == LUA_TNIL)
			rect_list = nullptr;
		else
			rect_list = DeckRectangleList::from_stack(L, 3);

		m_hotspots = rect_list;
		LuaHelpers::newindex_store_in_instance_table(L);
	}
	else if (key.starts_with("on_"))
	{
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

void ConnectorWindow::emit_event(lua_State* L, char const* func_name)
{
	lua_getfield(L, 1, func_name);
	if (lua_type(L, -1) == LUA_TFUNCTION)
	{
		lua_pushvalue(L, 1);
		LuaHelpers::pcall(L, 1, 0);
	}
	else
	{
		lua_pop(L, 1);
	}
}

int ConnectorWindow::_sdl_event_filter(void* userdata, SDL_Event* event)
{
	// This may run out of context, so we must catch and queue these events locally using atomics

	ConnectorWindow* self  = reinterpret_cast<ConnectorWindow*>(userdata);
	Uint32 const window_id = self->m_window ? SDL_GetWindowID(self->m_window) : -1;

	if (event->type == SDL_MOUSEMOTION && event->motion.windowID == window_id)
	{
		DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window mouse motion at ", event->motion.x, ',', event->motion.y);
		std::lock_guard guard(self->m_mutex);
		self->m_pending_events.push_back(*event);
	}
	else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.windowID == window_id)
	{
		DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window mouse button ", int(event->button.button), " down at ", event->button.x, ',', event->button.y);
		std::lock_guard guard(self->m_mutex);
		self->m_pending_events.push_back(*event);
	}
	else if (event->type == SDL_MOUSEBUTTONUP && event->button.windowID == window_id)
	{
		DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window mouse button ", int(event->button.button), " up at ", event->button.x, ',', event->button.y);
		std::lock_guard guard(self->m_mutex);
		self->m_pending_events.push_back(*event);
	}
	else if (event->type == SDL_WINDOWEVENT && event->window.windowID == window_id)
	{
		std::lock_guard guard(self->m_mutex);
		self->m_pending_events.push_back(*event);
	}

	return 0;
}

void ConnectorWindow::handle_window_event(lua_State* L, SDL_Event const& event)
{
	switch (event.window.event)
	{
		case SDL_WINDOWEVENT_EXPOSED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window redraw requested");
			m_event_surface_dirty = true;
			break;
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window changed size to ", event.window.data1, 'x', event.window.data2);
			m_event_surface_dirty = true;
			emit_event(L, "on_resize");
			break;
		case SDL_WINDOWEVENT_SHOWN:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window became shown");
			m_event_surface_dirty = true;
			break;
		case SDL_WINDOWEVENT_HIDDEN:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window became hidden");
			break;
		case SDL_WINDOWEVENT_MOVED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window moved to ", event.window.data1, 'x', event.window.data2);
			break;
		case SDL_WINDOWEVENT_RESIZED:
			// DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window resized to ", event.window.data1, 'x', event.window.data2);
			break;
		case SDL_WINDOWEVENT_MINIMIZED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window became minimized");
			break;
		case SDL_WINDOWEVENT_MAXIMIZED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window became maximized");
			break;
		case SDL_WINDOWEVENT_RESTORED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window became restored");
			m_event_surface_dirty = true;
			break;
		case SDL_WINDOWEVENT_ENTER:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window received pointer focus");
			break;
		case SDL_WINDOWEVENT_LEAVE:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window lost pointer focus");
			break;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window became focused");
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window became unfocused");
			break;
		case SDL_WINDOWEVENT_CLOSE:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window got request to close");
			break;
		case SDL_WINDOWEVENT_TAKE_FOCUS:
		case SDL_WINDOWEVENT_HIT_TEST:
		case SDL_WINDOWEVENT_ICCPROF_CHANGED:
		case SDL_WINDOWEVENT_DISPLAY_CHANGED:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window event with type ", event.window.event);
			break;
		default:
			DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window UNKNOWN event with type ", event.window.event);
			break;
	}
}

void ConnectorWindow::handle_motion_event(lua_State* L, SDL_Event const& event)
{
	lua_getfield(L, 1, "on_mouse_motion");
	if (lua_type(L, -1) == LUA_TFUNCTION)
	{
		lua_pushvalue(L, 1);
		lua_pushinteger(L, event.motion.x);
		lua_pushinteger(L, event.motion.y);

		lua_getfield(L, 1, "hotspots");
		if (DeckRectangleList* rect_list = DeckRectangleList::from_stack(L, -1, false); rect_list)
		{
			rect_list->push_any_contains(L, event.motion.x, event.motion.y);
			lua_replace(L, -2);
		}
		else
		{
			lua_pop(L, 1);
			lua_pushnil(L);
		}

		LuaHelpers::pcall(L, 4, 0);
	}
	else
	{
		lua_pop(L, 1);
	}
}

void ConnectorWindow::handle_button_event(lua_State* L, SDL_Event const& event)
{
	lua_getfield(L, 1, "on_mouse_button");
	if (lua_type(L, -1) == LUA_TFUNCTION)
	{
		lua_pushvalue(L, 1);
		lua_pushinteger(L, event.button.x);
		lua_pushinteger(L, event.button.y);
		lua_pushinteger(L, event.button.button);
		lua_pushboolean(L, event.type == SDL_MOUSEBUTTONDOWN);

		lua_getfield(L, 1, "hotspots");
		if (DeckRectangleList* rect_list = DeckRectangleList::from_stack(L, -1, false); rect_list)
		{
			rect_list->push_any_contains(L, event.motion.x, event.motion.y);
			lua_replace(L, -2);
		}
		else
		{
			lua_pop(L, 1);
			lua_pushnil(L);
		}

		LuaHelpers::pcall(L, 6, 0);
	}
	else
	{
		lua_pop(L, 1);
	}
}
