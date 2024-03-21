#include "connector_window.h"
#include "deck_card.h"
#include "deck_logger.h"
#include "lua_helpers.h"

template class ConnectorBase<ConnectorWindow>;

char const* ConnectorWindow::LUA_TYPENAME = "deck:ConnectorWindow";

ConnectorWindow::ConnectorWindow()
    : m_window(nullptr)
    , m_wanted_width(1600)
    , m_wanted_height(900)
    , m_wanted_visible(true)
    , m_window_size_is_ok(ATOMIC_FLAG_INIT)
    , m_window_surface_is_ok(ATOMIC_FLAG_INIT)
    , m_card(nullptr)
{
	m_window_size_is_ok.test_and_set(std::memory_order_relaxed);
	m_window_surface_is_ok.test_and_set(std::memory_order_relaxed);
}

ConnectorWindow::~ConnectorWindow()
{
	if (m_window)
		SDL_DestroyWindow(m_window);
}

void ConnectorWindow::tick_inputs(lua_State* L, lua_Integer clock)
{
	if (!m_window && !attempt_create_window(L))
		return;

	if (!m_window_size_is_ok.test_and_set(std::memory_order_acq_rel))
	{
		m_window_surface_is_ok.clear(std::memory_order_release);
		emit_event(L, "on_resize");
	}
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

	if (!m_window_surface_is_ok.test_and_set(std::memory_order_acq_rel))
	{
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
}

int ConnectorWindow::index(lua_State* L, std::string_view const& key) const
{
	if (key == "title")
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

		Uint32 flags = SDL_WINDOW_RESIZABLE;
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
			m_window_size_is_ok.test_and_set(std::memory_order_acq_rel);
			m_window_surface_is_ok.clear(std::memory_order_release);
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

	ConnectorWindow* self = reinterpret_cast<ConnectorWindow*>(userdata);
	if (event->type == SDL_WINDOWEVENT && event->window.windowID == SDL_GetWindowID(self->m_window))
	{
		switch (event->window.event)
		{
			case SDL_WINDOWEVENT_EXPOSED:
				DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window redraw requested");
				break;
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window changed size to ", event->window.data1, 'x', event->window.data2);
				self->m_window_size_is_ok.clear(std::memory_order_release);
				break;
			case SDL_WINDOWEVENT_SHOWN:
				DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window became shown");
				self->m_window_surface_is_ok.clear(std::memory_order_release);
				break;
			case SDL_WINDOWEVENT_HIDDEN:
				DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window became hidden");
				break;
			case SDL_WINDOWEVENT_MOVED:
				DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window moved to ", event->window.data1, 'x', event->window.data2);
				break;
			case SDL_WINDOWEVENT_RESIZED:
				// DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window resized to ", event->window.data1, 'x', event->window.data2);
				break;
			case SDL_WINDOWEVENT_MINIMIZED:
				DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window became minimized");
				break;
			case SDL_WINDOWEVENT_MAXIMIZED:
				DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window became maximized");
				break;
			case SDL_WINDOWEVENT_RESTORED:
				DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window became restored");
				self->m_window_surface_is_ok.clear(std::memory_order_release);
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
				DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window event with type ", event->window.event);
				break;
			default:
				DeckLogger::log_message(nullptr, DeckLogger::Level::Debug, "Window UNKNOWN event with type ", event->window.event);
				break;
		}
	}

	return 0;
}
