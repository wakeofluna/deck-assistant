#include "connector_elgato_streamdeck.h"
#include "SDL_error.h"
#include "deck_card.h"
#include "deck_connector.h"
#include "deck_image.h"
#include "deck_logger.h"
#include <algorithm>
#include <cassert>
#include <codecvt>
#include <cstring>
#include <fstream>
#include <iostream>
#include <locale>
#include <sstream>

namespace
{

constexpr unsigned char const INVALID_BRIGHTNESS = 255;

constexpr std::string_view const READONLY_KEYS[] = {
	"connected",
	"error",
	"vid",
	"pid",
	"model",
	"serialnumber",
};
constexpr std::size_t const READONLY_KEYS_SIZE = sizeof(READONLY_KEYS) / sizeof(*READONLY_KEYS);

bool is_readonly(std::string_view const& key)
{
	for (std::size_t idx = 0; idx < READONLY_KEYS_SIZE; ++idx)
		if (READONLY_KEYS[idx] == key)
			return true;
	return false;
}

constexpr std::pair<int, std::string_view> const MODELS[] = {
	{0x0060,  "Stream Deck Original"},
	{ 0x006d, "Stream Deck V2"      },
	{ 0x0063, "Stream Deck Mini"    },
	{ 0x006c, "Stream Deck XL"      },
};
constexpr std::size_t const MODELS_SIZE = sizeof(MODELS) / sizeof(*MODELS);

std::string_view get_model(int pid)
{
	for (std::size_t idx = 0; idx < MODELS_SIZE; ++idx)
		if (MODELS[idx].first == pid)
			return MODELS[idx].second;
	return std::string_view();
}

std::string convert(wchar_t const* wstr)
{
	static std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
	return convert.to_bytes(wstr);
}

void convert_button_table(lua_State* L, int idx)
{
	std::vector<bool> buttons;
	buttons.reserve(32);

	for (std::size_t i = 1; i == buttons.size() + 1; ++i)
	{
		lua_rawgeti(L, idx, i);
		if (lua_isboolean(L, -1))
			buttons.push_back(lua_toboolean(L, -1));
		lua_pop(L, 1);
	}

	luaL_Buffer buf;
	luaL_buffinit(L, &buf);

	luaL_addchar(&buf, '{');
	for (std::size_t idx = 0; idx < buttons.size(); ++idx)
	{
		if (idx > 0)
			luaL_addchar(&buf, ',');
		if (buttons[idx])
			luaL_addlstring(&buf, "true", 4);
		else
			luaL_addlstring(&buf, "false", 5);
	}
	luaL_addchar(&buf, '}');
	luaL_pushresult(&buf);
}

} // namespace

char const* ConnectorElgatoStreamDeck::SUBTYPE_NAME = "Elgato StreamDeck";

ConnectorElgatoStreamDeck::ConnectorElgatoStreamDeck()
    : m_hid_device(nullptr)
    , m_hid_last_scan(-1)
    , m_wanted_brightness(INVALID_BRIGHTNESS)
    , m_actual_brightness(INVALID_BRIGHTNESS)
{
}

ConnectorElgatoStreamDeck::~ConnectorElgatoStreamDeck()
{
	if (m_hid_device)
		SDL_hid_close(m_hid_device);
}

char const* ConnectorElgatoStreamDeck::get_subtype_name() const
{
	return SUBTYPE_NAME;
}

void ConnectorElgatoStreamDeck::tick(lua_State* L, int delta_msec)
{
	if (!m_hid_device)
	{
		std::uint32_t changes = SDL_hid_device_change_count();
		if (changes != m_hid_last_scan)
		{
			m_hid_last_scan = changes;
			attempt_connect_device();
		}

		if (!m_hid_device)
			return;

		lua_getfield(L, -1, "on_connect");
		if (lua_type(L, -1) == LUA_TFUNCTION)
		{
			if (!LuaHelpers::pcall(L, 0, 0))
			{
				std::stringstream buf;
				LuaHelpers::print_error_context(buf);
				DeckLogger::lua_log_message(L, DeckLogger::Level::Error, buf.str());
			}
		}
		else
		{
			lua_pop(L, 1);
		}
	}

	if (m_actual_brightness != m_wanted_brightness && m_wanted_brightness != INVALID_BRIGHTNESS)
		write_brightness(m_wanted_brightness);

	if (update_button_state())
	{
		m_buttons_state.resize(m_buttons_new_state.size());

		lua_createtable(L, m_buttons_new_state.size(), 0);
		for (std::size_t idx = 0; idx < m_buttons_new_state.size(); ++idx)
		{
			lua_pushboolean(L, m_buttons_new_state[idx]);
			lua_rawseti(L, -2, idx + 1);
		}

		for (std::size_t idx = 0; idx < m_buttons_state.size(); ++idx)
		{
			if (m_buttons_state[idx] != m_buttons_new_state[idx])
			{
				lua_getfield(L, -2, m_buttons_new_state[idx] ? "on_press" : "on_release");
				if (lua_type(L, -1) == LUA_TFUNCTION)
				{
					lua_pushinteger(L, idx + 1);
					lua_pushvalue(L, -3);
					lua_pcall(L, 2, 0, 0);
				}
				else
				{
					lua_pop(L, 1);
				}
			}
			m_buttons_state[idx] = m_buttons_new_state[idx];
		}

		lua_pop(L, 1);
	}

	if (!m_hid_device)
	{
		lua_getfield(L, -1, "on_disconnect");
		if (lua_type(L, -1) == LUA_TFUNCTION)
		{
			lua_pcall(L, 0, 0, 0);
		}
		else
		{
			lua_pop(L, 1);
		}
	}
}

void ConnectorElgatoStreamDeck::shutdown(lua_State* L)
{
	if (m_hid_device)
	{
		SDL_hid_close(m_hid_device);
		m_hid_device = nullptr;
	}
}

void ConnectorElgatoStreamDeck::init_instance_table(lua_State* L)
{
	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, &ConnectorElgatoStreamDeck::_lua_default_on_connect, 1);
	lua_setfield(L, -2, "on_connect");

	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, &ConnectorElgatoStreamDeck::_lua_default_on_disconnect, 1);
	lua_setfield(L, -2, "on_disconnect");

	lua_pushcfunction(L, &ConnectorElgatoStreamDeck::_lua_default_on_press);
	lua_setfield(L, -2, "on_press");

	lua_pushcfunction(L, &ConnectorElgatoStreamDeck::_lua_default_on_release);
	lua_setfield(L, -2, "on_release");

	lua_pushcfunction(L, &ConnectorElgatoStreamDeck::_lua_set_button);
	lua_setfield(L, -2, "set_button");
}

int ConnectorElgatoStreamDeck::index(lua_State* L) const
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
		if (key == "brightness")
		{
			if (m_wanted_brightness != INVALID_BRIGHTNESS)
				lua_pushinteger(L, m_wanted_brightness);
		}
		else if (key == "connected")
		{
			lua_pushboolean(L, m_hid_device != nullptr);
		}
		else if (key == "error")
		{
			if (!m_last_error.empty())
				lua_pushlstring(L, m_last_error.data(), m_last_error.size());
		}
		else if (key == "vid")
		{
			if (m_vid)
				lua_pushinteger(L, m_vid);
		}
		else if (key == "pid")
		{
			if (m_pid)
				lua_pushinteger(L, m_pid);
		}
		else if (key == "model")
		{
			std::string_view model = get_model(m_pid);
			if (!model.empty())
				lua_pushlstring(L, model.data(), model.size());
		}
		else if (key == "serialnumber")
		{
			if (!m_serialnumber.empty())
				lua_pushlstring(L, m_serialnumber.data(), m_serialnumber.size());
		}
	}

	return lua_gettop(L) == 2 ? 0 : 1;
}

int ConnectorElgatoStreamDeck::newindex(lua_State* L)
{
	lua_settop(L, 3);

	if (lua_type(L, 2) == LUA_TSTRING)
	{
		std::string_view key = LuaHelpers::check_arg_string(L, 2, false);

		if (is_readonly(key))
		{
			luaL_error(L, "Connector \"%s\" key \"%s\" is readonly", SUBTYPE_NAME, key.data());
			return 0;
		}
		else if (key == "brightness")
		{
			lua_Integer value   = LuaHelpers::check_arg_int(L, 3);
			m_wanted_brightness = std::clamp<int>(value, 0, 100);
			return 0;
		}
		else if (key.starts_with("on_"))
		{
			int const vtype = lua_type(L, 3);
			if (vtype != LUA_TFUNCTION && vtype != LUA_TNIL)
				luaL_typerror(L, 3, "event handlers must be functions");

			lua_rawset(L, 1);
			return 0;
		}
	}

	lua_rawset(L, 1);
	return 0;
}

int ConnectorElgatoStreamDeck::_lua_default_on_connect(lua_State* L)
{
	ConnectorElgatoStreamDeck* self = reinterpret_cast<ConnectorElgatoStreamDeck*>(lua_touserdata(L, lua_upvalueindex(1)));
	std::string_view model          = get_model(self->m_pid);
	std::string_view serialnumber   = self->m_serialnumber;

	lua_getglobal(L, "print");

	luaL_Buffer buf;
	luaL_buffinit(L, &buf);
	luaL_addstring(&buf, "Elgato StreamDeck on_connect(): ");
	luaL_addlstring(&buf, model.data(), model.size());
	luaL_addstring(&buf, " serialnumber ");
	luaL_addlstring(&buf, serialnumber.data(), serialnumber.size());
	luaL_pushresult(&buf);

	lua_pcall(L, 1, 0, 0);
	return 0;
}

int ConnectorElgatoStreamDeck::_lua_default_on_disconnect(lua_State* L)
{
	ConnectorElgatoStreamDeck* self = reinterpret_cast<ConnectorElgatoStreamDeck*>(lua_touserdata(L, lua_upvalueindex(1)));

	lua_getglobal(L, "print");

	luaL_Buffer buf;
	luaL_buffinit(L, &buf);
	luaL_addstring(&buf, "Elgato StreamDeck on_disconnect(): ");
	luaL_addlstring(&buf, self->m_last_error.data(), self->m_last_error.size());
	luaL_pushresult(&buf);

	lua_pcall(L, 1, 0, 0);
	return 0;
}

int ConnectorElgatoStreamDeck::_lua_default_on_press(lua_State* L)
{
	lua_getglobal(L, "print");

	luaL_Buffer buf;
	luaL_buffinit(L, &buf);
	luaL_addstring(&buf, "Elgato StreamDeck on_press(): ");
	lua_pushvalue(L, 1);
	luaL_addvalue(&buf);
	luaL_addchar(&buf, ' ');
	convert_button_table(L, 2);
	luaL_addvalue(&buf);
	luaL_pushresult(&buf);

	lua_pcall(L, 1, 0, 0);
	return 0;
}

int ConnectorElgatoStreamDeck::_lua_default_on_release(lua_State* L)
{
	lua_getglobal(L, "print");

	luaL_Buffer buf;
	luaL_buffinit(L, &buf);
	luaL_addstring(&buf, "Elgato StreamDeck on_release(): ");
	lua_pushvalue(L, 1);
	luaL_addvalue(&buf);
	luaL_addchar(&buf, ' ');
	convert_button_table(L, 2);
	luaL_addvalue(&buf);
	luaL_pushresult(&buf);

	lua_pcall(L, 1, 0, 0);
	return 0;
}

int ConnectorElgatoStreamDeck::_lua_set_button(lua_State* L)
{
	ConnectorElgatoStreamDeck* self = static_cast<ConnectorElgatoStreamDeck*>(from_stack(L, 1, SUBTYPE_NAME));
	unsigned char button            = LuaHelpers::check_arg_int(L, 2);

	if (button < 1)
	{
		luaL_argerror(L, 2, "buttons start counting at 1");
	}
	else if (!self->m_hid_device)
	{
		luaL_error(L, "Device is not connected");
	}
	else if (DeckImage* image = DeckImage::from_stack(L, 3, false); image)
	{
		SDL_Surface* surface = image->get_surface();
		if (surface)
			self->set_button(button, surface);
	}
	else if (DeckCard* card = DeckCard::from_stack(L, 3, false); card)
	{
		SDL_Surface* surface = card->get_surface();
		if (surface)
			self->set_button(button, surface);
	}
	else
	{
		luaL_typerror(L, 3, "deck:Image or deck:Card");
	}

	return 0;
}

void ConnectorElgatoStreamDeck::attempt_connect_device()
{
	if (m_hid_device)
		return;

	SDL_hid_device_info* device_list = SDL_hid_enumerate(0x0fd9, 0);
	if (!device_list)
	{
		m_last_error = "HID enumerate failed";
		return;
	}

	bool found_any = false;

	for (SDL_hid_device_info* info = device_list; info; info = info->next)
	{
		std::string_view model = get_model(info->product_id);
		if (!model.empty())
		{
			found_any = true;

			std::string device_serialnumber = convert(info->serial_number);
			if (!m_filter_serialnumber.empty() && device_serialnumber != m_filter_serialnumber)
			{
				m_last_error = "Device ignored due to serialnumber mismatch (expected " + m_filter_serialnumber + ')';
				continue;
			}

			if (!m_hid_device)
			{
				m_hid_device = SDL_hid_open_path(info->path, false);
				if (!m_hid_device)
				{
					m_last_error  = "Open failed: ";
					m_last_error += SDL_GetError();
				}
				else
				{
					m_serialnumber.swap(device_serialnumber);
					m_vid         = info->vendor_id;
					m_pid         = info->product_id;
					m_button_size = (info->product_id == 0x006c) ? 96 : 72;
					break;
				}
			}
		}
	}

	SDL_hid_free_enumeration(device_list);

	if (!found_any)
		m_last_error = "No suitable devices found";

	if (m_hid_device)
		m_last_error.clear();
}

void ConnectorElgatoStreamDeck::write_brightness(unsigned char value)
{
	if (m_hid_device && value != INVALID_BRIGHTNESS)
	{
		if (value > 100)
			value = 100;

		m_buffer.fill(0);
		m_buffer[0] = 0x03;
		m_buffer[1] = 0x08;
		m_buffer[2] = value;

		int result = SDL_hid_send_feature_report(m_hid_device, m_buffer.data(), 32);
		if (result == -1)
		{
			m_last_error  = "Send feature report failed: ";
			m_last_error += SDL_GetError();
			force_disconnect();
		}
		else
		{
			m_actual_brightness = value;
		}
	}
}

void ConnectorElgatoStreamDeck::write_image_data(unsigned char button, std::vector<unsigned char> const& bytes)
{
	if (!m_hid_device)
		return;

	int const max_payload_size = 1024 - 8;

	std::size_t total_sent  = 0;
	std::uint16_t iteration = 0;

	while (total_sent < bytes.size())
	{
		std::size_t remaining      = bytes.size() - total_sent;
		bool last_packet           = remaining <= max_payload_size;
		std::uint16_t slice_length = last_packet ? remaining : max_payload_size;

		m_buffer[0] = 0x02;
		m_buffer[1] = 0x07;
		m_buffer[2] = button;
		m_buffer[3] = last_packet ? 1 : 0;
		m_buffer[4] = slice_length & 0xff;
		m_buffer[5] = slice_length >> 8;
		m_buffer[6] = iteration & 0xff;
		m_buffer[7] = iteration >> 8;

		std::memcpy(&m_buffer.at(8), &bytes.at(total_sent), slice_length);
		if (slice_length < max_payload_size)
			std::memset(&m_buffer.at(8 + slice_length), 0, max_payload_size - slice_length);

		std::size_t sent = 0;
		while (sent < m_buffer.size())
		{
			int result = SDL_hid_write(m_hid_device, m_buffer.data() + sent, m_buffer.size() - sent);
			if (result <= 0)
			{
				m_last_error = "HID write failed";
				force_disconnect();
				break;
			}
			sent += result;
		}

		total_sent += slice_length;
		++iteration;
	}
}

void ConnectorElgatoStreamDeck::set_button(unsigned char button, SDL_Surface* surface)
{
	if (!surface)
		return;

	std::vector<unsigned char> bytes;

	// Elgato has the buttons rotated 180 degrees so we need to make a copy and shuffle the pixels...
	// There's a chance the surfaces are not continguous, so we have to reverse all pixel data per row
	SDL_Surface* new_surface;

	if (surface->w == m_button_size && surface->h == m_button_size)
	{
		new_surface = SDL_CreateRGBSurfaceWithFormat(0, m_button_size, m_button_size, 32, SDL_PIXELFORMAT_ARGB32);

		unsigned char* source_data = reinterpret_cast<unsigned char*>(surface->pixels);
		unsigned char* target_data = reinterpret_cast<unsigned char*>(new_surface->pixels) + (new_surface->h * new_surface->pitch);

		for (int y = 0; y < m_button_size; ++y)
		{
			target_data -= new_surface->pitch;

			std::uint32_t const* source = reinterpret_cast<std::uint32_t*>(source_data);
			std::uint32_t* target       = reinterpret_cast<std::uint32_t*>(target_data);
			std::reverse_copy(source, source + m_button_size, target);

			source_data += surface->pitch;
		}
	}
	else
	{
		new_surface = DeckImage::resize_surface(surface, m_button_size, m_button_size);

		unsigned char* start_data = reinterpret_cast<unsigned char*>(new_surface->pixels);
		unsigned char* end_data   = start_data + (new_surface->h * new_surface->pitch);

		// Note that the center line will be skipped if the image is odd-sized, but no known button sizes are affected anyway

		for (int y = 0; y < new_surface->h / 2; ++y)
		{
			end_data -= new_surface->pitch;

			std::uint32_t* front_row    = reinterpret_cast<std::uint32_t*>(start_data);
			std::uint32_t* back_row     = reinterpret_cast<std::uint32_t*>(end_data);
			std::uint32_t* back_row_end = back_row + new_surface->w;

			while (back_row != back_row_end)
			{
				--back_row_end;
				std::swap(*front_row, *back_row_end);
				++front_row;
			}

			start_data += new_surface->pitch;
		}
	}

	bytes = DeckImage::save_surface_as_jpeg(new_surface);
	SDL_FreeSurface(new_surface);

	if (!bytes.empty())
		write_image_data(button - 1, bytes);
}

bool ConnectorElgatoStreamDeck::update_button_state()
{
	if (m_hid_device)
	{
		int len = SDL_hid_read_timeout(m_hid_device, m_buffer.data(), m_buffer.size(), 0);
		if (len == -1)
		{
			m_last_error = "HID read failed";
			force_disconnect();
		}
		else if (len >= 4 && m_buffer[0] == 0x01) // button report
		{
			std::uint16_t num_buttons = m_buffer[2] + (m_buffer[3] << 8);
			if (len >= 4 + num_buttons)
			{
				m_buttons_new_state.resize(num_buttons);
				for (std::uint16_t idx = 0; idx < num_buttons; ++idx)
				{
					bool new_state           = m_buffer[4 + idx] != 0;
					m_buttons_new_state[idx] = new_state;
				}
				return true;
			}
		}
	}
	return false;
}

void ConnectorElgatoStreamDeck::force_disconnect()
{
	if (m_hid_device)
	{
		SDL_hid_close(m_hid_device);
		m_hid_device = nullptr;
	}
}
