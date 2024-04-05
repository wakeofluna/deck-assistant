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

#ifndef DECK_ASSISTANT_DECK_CONNECTOR_FACTORY_H
#define DECK_ASSISTANT_DECK_CONNECTOR_FACTORY_H

#include "lua_class.h"

class DeckConnectorFactory : public LuaClass<DeckConnectorFactory>
{
public:
	static char const* LUA_TYPENAME;

	static void init_class_table(lua_State* L);
	int newindex(lua_State* L);
	int tostring(lua_State* L) const;
};

#endif // DECK_ASSISTANT_DECK_CONNECTOR_FACTORY_H
