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

#include <SDL_rwops.h>
#include <string_view>

namespace builtins
{

// List of generated builtins
extern std::basic_string_view<unsigned char> builtins_lua;
extern std::basic_string_view<unsigned char> connectors_lua;
extern std::basic_string_view<unsigned char> deck_assistant_svg;
extern std::basic_string_view<unsigned char> main_window_script_lua;
extern std::basic_string_view<unsigned char> oauth2_callback_html;
extern std::basic_string_view<unsigned char> vera_ttf;

inline std::basic_string_view<unsigned char> font()
{
	return vera_ttf;
}

inline std::string_view builtins_script()
{
	return std::string_view((char const*)builtins_lua.data(), builtins_lua.size());
}

inline std::string_view connectors_script()
{
	return std::string_view((char const*)connectors_lua.data(), connectors_lua.size());
}

inline std::string_view deck_assistant_icon()
{
	return std::string_view((char const*)deck_assistant_svg.data(), deck_assistant_svg.size());
}

inline std::string_view main_window_script()
{
	return std::string_view((char const*)main_window_script_lua.data(), main_window_script_lua.size());
}

inline std::string_view oauth2_callback_page()
{
	return std::string_view((char const*)oauth2_callback_html.data(), oauth2_callback_html.size());
}

template <typename T>
inline SDL_RWops* as_rwops(std::basic_string_view<T> const& builtin)
{
	return SDL_RWFromConstMem(builtin.data(), builtin.size());
}

} // namespace builtins
