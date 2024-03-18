#include "connector_window.h"
#include "deck_logger.h"
#include "lua_helpers.h"

char const* ConnectorWindow::SUBTYPE_NAME = "Window";

ConnectorWindow::ConnectorWindow()
    : m_window(nullptr)
    , m_wanted_width(1600)
    , m_wanted_height(900)
    , m_wanted_visible(true)
{
}

ConnectorWindow::~ConnectorWindow()
{
	if (m_window)
		SDL_DestroyWindow(m_window);
}

char const* ConnectorWindow::get_subtype_name() const
{
	return SUBTYPE_NAME;
}

void ConnectorWindow::tick(lua_State* L, int delta_msec)
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
			DeckLogger::log_message(L, DeckLogger::Level::Error, "Failed to create window", SDL_GetError());
		}
		else
		{
			SDL_GetWindowSurface(m_window);
			SDL_UpdateWindowSurface(m_window);
		}
	}

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
}

void ConnectorWindow::shutdown(lua_State* L)
{
	if (m_window)
	{
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
	}
}

void ConnectorWindow::init_instance_table(lua_State* L)
{
}

int ConnectorWindow::index(lua_State* L) const
{
	lua_settop(L, 2);

	lua_pushvalue(L, 2);
	lua_rawget(L, 1);
	if (lua_type(L, -1) != LUA_TNIL)
		return 1;
	lua_pop(L, 1);

	if (lua_type(L, 2) == LUA_TSTRING)
	{
		std::string_view key = LuaHelpers::check_arg_string(L, 2, false);
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
	}

	return lua_gettop(L) == 2 ? 0 : 1;
}

int ConnectorWindow::newindex(lua_State* L)
{
	lua_settop(L, 3);

	if (lua_type(L, 2) == LUA_TSTRING)
	{
		std::string_view key = LuaHelpers::check_arg_string(L, 2, false);

		if (key == "title")
		{
			std::string_view title = LuaHelpers::check_arg_string(L, 3);
			m_wanted_title         = title;
			return 0;
		}

		if (key == "w" || key == "width")
		{
			int width = LuaHelpers::check_arg_int(L, 3);
			luaL_argcheck(L, (width > 0), 3, "WIDTH must be larger than zero");
			m_wanted_width = width;
			return 0;
		}

		if (key == "h" || key == "height")
		{
			int height = LuaHelpers::check_arg_int(L, 3);
			luaL_argcheck(L, (height > 0), 3, "HEIGHT must be larger than zero");
			m_wanted_height = height;
			return 0;
		}

		if (key == "visible")
		{
			luaL_argcheck(L, (lua_type(L, 3) == LUA_TBOOLEAN), 3, "visible must be a boolean");
			m_wanted_visible = lua_toboolean(L, 3);
			return 0;
		}
	}

	lua_rawset(L, 1);
	return 0;
}
