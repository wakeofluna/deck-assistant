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

#ifndef DECK_ASSISTANT_CONNECTOR_SPOUT_H
#define DECK_ASSISTANT_CONNECTOR_SPOUT_H

#ifdef HAVE_SPOUT

#include "connector_base.h"
#include <SDL.h>
#include <array>
#include <string>
#include <vector>

class DeckCard;
class DeckRectangleList;
class spoutDX;

class ConnectorSpout : public ConnectorBase<ConnectorSpout>
{
public:
	ConnectorSpout();
	~ConnectorSpout();

	void initial_setup(lua_State* L, bool is_reload) override;
	void tick_inputs(lua_State* L, lua_Integer clock) override;
	void tick_outputs(lua_State* L, lua_Integer clock) override;
	void shutdown(lua_State* L) override;

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);

private:
	void release_spout_sender();
	static int _lua_redraw(lua_State* L);

private:
	SDL_Surface* m_spout_surface;
	spoutDX* m_spout_sender;
	std::string m_spout_name;

	DeckCard* m_card;
	int m_wanted_width;
	int m_wanted_height;
	bool m_enabled;
	bool m_dirty;
	bool m_need_send;
	bool m_can_send;
};

#endif

#endif // DECK_ASSISTANT_CONNECTOR_SPOUT_H
