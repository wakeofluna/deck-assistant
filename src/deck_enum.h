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

#ifndef DECK_ASSISTANT_DECK_ENUM_H
#define DECK_ASSISTANT_DECK_ENUM_H

#include "lua_class.h"
#include <optional>
#include <string_view>
#include <type_traits>

class DeckEnum : public LuaClass<DeckEnum>
{
public:
	DeckEnum(std::string_view const& clz, std::string_view const& name, std::size_t value);
	DeckEnum(DeckEnum const&)            = delete;
	DeckEnum(DeckEnum&&)                 = delete;
	DeckEnum& operator=(DeckEnum const&) = delete;
	DeckEnum& operator=(DeckEnum&&)      = delete;

	static DeckEnum* get_or_create(lua_State* L, std::string_view const& enum_class, std::string_view const& value_name, std::size_t value);

	template <typename T>
	    requires std::is_enum_v<T>
	static inline DeckEnum* get_or_create(lua_State* L, std::string_view const& enum_class, std::string_view const& value_name, T value)
	{
		return get_or_create(L, enum_class, value_name, static_cast<std::size_t>(value));
	}

	static std::optional<int> to_int(lua_State* L, int idx, std::string_view const& enum_class, bool throw_error = true);

	template <typename T>
	    requires std::is_enum_v<T>
	static std::optional<T> to_enum(lua_State* L, int idx, std::string_view const& enum_class, bool throw_error = true)
	{
		std::optional<int> tmp = to_int(L, idx, enum_class, throw_error);
		return tmp ? std::make_optional(static_cast<T>(tmp.value())) : std::nullopt;
	}

	template <typename T>
	    requires std::is_enum_v<T>
	inline std::optional<T> to_enum(std::string_view const& enum_class) const
	{
		return m_class == enum_class ? std::make_optional(static_cast<T>(m_value)) : std::nullopt;
	}

	static constexpr bool const LUA_ENABLE_PUSH_THIS = true;
	static char const* LUA_TYPENAME;
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L);
	int tostring(lua_State* L) const;

	inline std::string_view class_name() const { return m_class; }
	inline std::string_view value_name() const { return m_name; }
	inline std::size_t value() const { return m_value; }

private:
	std::string_view m_class;
	std::string_view m_name;
	std::size_t m_value;
};

#endif // DECK_ASSISTANT_DECK_ENUM_H
