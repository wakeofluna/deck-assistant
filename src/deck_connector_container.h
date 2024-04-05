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

#ifndef DECK_ASSISTANT_DECK_CONNECTOR_CONTAINER_H
#define DECK_ASSISTANT_DECK_CONNECTOR_CONTAINER_H

#include "lua_class.h"

class DeckConnectorContainer : public LuaClass<DeckConnectorContainer>
{
public:
	static void for_each(lua_State* L, char const* function_name, int nargs);

	static char const* LUA_TYPENAME;
	void init_instance_table(lua_State* L);
	int newindex(lua_State* L);
	int tostring(lua_State* L) const;
};

#endif // DECK_ASSISTANT_DECK_CONNECTOR_CONTAINER_H
