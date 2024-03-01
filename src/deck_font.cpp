#include "deck_font.h"
#include "lua_class.hpp"

template class LuaClass<DeckFont>;

char const* DeckFont::LUA_TYPENAME = "deck-assistant.DeckFont";

DeckFont::DeckFont()
    : m_font_name("Sans")
    , m_font_size(12)
{
}

DeckFont::~DeckFont() = default;

void DeckFont::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &DeckFont::_lua_clone);
	lua_pushvalue(L, -1);
	lua_setfield(L, -3, "clone");
	lua_setfield(L, -2, "dup");
}

int DeckFont::index(lua_State* L, std::string_view const& key) const
{
	if (key == "font")
	{
		lua_pushlstring(L, m_font_name.c_str(), m_font_name.size());
	}
	else if (key == "size")
	{
		lua_pushinteger(L, m_font_size);
	}
	else
	{
		luaL_error(L, "invalid key for DeckFont (allowed: font, size)");
	}
	return 1;
}

int DeckFont::newindex(lua_State* L, std::string_view const& key)
{
	if (key == "font")
	{
		std::string_view value = LuaHelpers::check_arg_string(L, -1);
		m_font_name            = value;
	}
	else if (key == "size")
	{
		int value   = LuaHelpers::check_arg_int(L, -1);
		m_font_size = value;
	}
	else if (key == "definition")
	{
		int const vtype = lua_type(L, -1);
		if (vtype == LUA_TSTRING)
		{
			std::string_view value = LuaHelpers::check_arg_string(L, -1);
			m_font_name            = value;
		}
		else if (vtype == LUA_TNUMBER)
		{
			int value   = LuaHelpers::check_arg_int(L, -1);
			m_font_size = value;
		}
		else if (vtype == LUA_TTABLE)
		{
			lua_remove(L, 2);
			lua_settop(L, 2);
			copy_table_fields(L);
		}
		else
		{
			luaL_typerror(L, absidx(L, -1), "string, number or table");
		}
	}
	else
	{
		luaL_argerror(L, absidx(L, -2), "invalid key for DeckFont (allowed: font, size)");
	}
	return 0;
}

int DeckFont::to_string(lua_State* L) const
{
	lua_pushfstring(L, "%s: %p: %s %d", type_name(), this, m_font_name.c_str(), int(m_font_size));
	return 1;
}

int DeckFont::_lua_clone(lua_State* L)
{
	DeckFont* self = from_stack(L, 1);

	DeckFont* new_font    = DeckFont::create_new(L);
	new_font->m_font_name = self->m_font_name;
	new_font->m_font_size = self->m_font_size;

	return 1;
}
