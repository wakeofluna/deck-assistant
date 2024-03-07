#ifdef HAVE_HIDAPI

#include "connector_elgato_streamdeck.h"
#include <algorithm>
#include <codecvt>
#include <hidapi.h>
#include <iostream>
#include <locale>

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

	for (lua_Integer i = 1; i == buttons.size() + 1; ++i)
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

/*
constexpr char nibble(unsigned char ch)
{
    if (ch < 10)
        return '0' + ch;
    else
        return 'a' + ch - 10;
}

void dump(std::ostream& stream, unsigned char const* data, std::size_t len)
{
    for (std::size_t i = 0; i < len; ++i)
    {
        char const ch = data[i];
        stream << nibble(ch >> 8) << nibble(ch & 0x0f) << ' ';
    }
    stream << std::endl;
}
*/

} // namespace

char const* ConnectorElgatoStreamDeck::SUBTYPE_NAME = "Elgato StreamDeck";

ConnectorElgatoStreamDeck::ConnectorElgatoStreamDeck()
    : m_hid_device(nullptr)
    , m_delta_since_last_scan(-1000)
    , m_wanted_brightness(INVALID_BRIGHTNESS)
    , m_actual_brightness(INVALID_BRIGHTNESS)
{
}

ConnectorElgatoStreamDeck::~ConnectorElgatoStreamDeck()
{
	if (m_hid_device)
		hid_close(m_hid_device);
}

char const* ConnectorElgatoStreamDeck::get_subtype_name() const
{
	return SUBTYPE_NAME;
}

void ConnectorElgatoStreamDeck::tick(lua_State* L, int delta_msec)
{
	m_delta_since_last_scan += delta_msec;

	if (!m_hid_device && m_delta_since_last_scan >= 1000)
	{
		m_delta_since_last_scan = 0;
		attempt_connect_device();

		if (m_hid_device)
		{
			lua_getfield(L, -1, "on_connect");
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

	if (!m_hid_device)
		return;

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
		hid_close(m_hid_device);
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
			hid_device_info* info = m_hid_device ? hid_get_device_info(m_hid_device) : nullptr;
			if (info)
				lua_pushinteger(L, info->vendor_id);
		}
		else if (key == "pid")
		{
			hid_device_info* info = m_hid_device ? hid_get_device_info(m_hid_device) : nullptr;
			if (info)
				lua_pushinteger(L, info->product_id);
		}
		else if (key == "model")
		{
			hid_device_info* info = m_hid_device ? hid_get_device_info(m_hid_device) : nullptr;
			if (info)
			{
				std::string_view model = get_model(info->product_id);
				if (!model.empty())
				{
					lua_pushlstring(L, model.data(), model.size());
				}
				else
				{
					std::string product = convert(info->product_string);
					lua_pushlstring(L, product.data(), product.size());
				}
			}
		}
		else if (key == "serialnumber")
		{
			hid_device_info* info = m_hid_device ? hid_get_device_info(m_hid_device) : nullptr;
			if (info)
			{
				std::string serialnumber = convert(info->serial_number);
				lua_pushlstring(L, serialnumber.data(), serialnumber.size());
			}
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
	hid_device_info* info           = self->m_hid_device ? hid_get_device_info(self->m_hid_device) : nullptr;
	std::string_view model          = info ? get_model(info->product_id) : "(error)";
	std::string serialnumber        = info ? convert(info->serial_number) : "(error)";

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

void ConnectorElgatoStreamDeck::attempt_connect_device()
{
	if (m_hid_device)
		return;

	hid_device_info* device_list = hid_enumerate(0x0fd9, 0);
	if (!device_list)
	{
		m_last_error = "Enumerate failed: " + convert(hid_error(NULL));
		return;
	}

	bool found_any = false;

	for (hid_device_info* info = device_list; info; info = info->next)
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
				m_hid_device = hid_open_path(info->path);
				if (!m_hid_device)
				{
					m_last_error = "Open failed: " + convert(hid_error(nullptr));
				}
			}
		}
	}

	hid_free_enumeration(device_list);

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

		int result = hid_send_feature_report(m_hid_device, m_buffer.data(), 32);
		if (result == -1)
		{
			m_last_error = "Send feature report failed: " + convert(hid_error(m_hid_device));
			force_disconnect();
		}
		else
		{
			m_actual_brightness = value;
		}
	}
}

bool ConnectorElgatoStreamDeck::update_button_state()
{
	if (m_hid_device)
	{
		int len = hid_read_timeout(m_hid_device, m_buffer.data(), m_buffer.size(), 0);
		if (len == -1)
		{
			m_last_error = "HID read failed: " + convert(hid_error(m_hid_device));
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
		hid_close(m_hid_device);
		m_hid_device = nullptr;
	}
}

#endif
