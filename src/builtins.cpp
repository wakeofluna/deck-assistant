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

#include "builtins.h"

namespace
{

#include "builtins/vera_ttf.c"

}

namespace builtins
{

Blob font()
{
	return Blob(vera_ttf, vera_ttf_len);
}

SDL_RWops* as_rwops(Blob const& builtin)
{
	return SDL_RWFromConstMem(builtin.data(), builtin.size());
}

} // namespace builtins
