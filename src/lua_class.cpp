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

#include "lua_class.hpp"
#include "connector_elgato_streamdeck.h"
#include "connector_vnc.h"
#include "connector_websocket.h"
#include "connector_window.h"
#include "deck_card.h"
#include "deck_colour.h"
#include "deck_connector_container.h"
#include "deck_connector_factory.h"
#include "deck_font.h"
#include "deck_logger.h"
#include "deck_module.h"
#include "deck_promise.h"
#include "deck_promise_list.h"
#include "deck_rectangle.h"
#include "deck_rectangle_list.h"
#include "deck_util.h"

template class LuaClass<ConnectorElgatoStreamDeck>;
template class LuaClass<ConnectorWebsocket>;
template class LuaClass<ConnectorWindow>;
template class LuaClass<DeckCard>;
template class LuaClass<DeckConnectorContainer>;
template class LuaClass<DeckConnectorFactory>;
template class LuaClass<DeckColour>;
template class LuaClass<DeckFont>;
template class LuaClass<DeckLogger>;
template class LuaClass<DeckModule>;
template class LuaClass<DeckPromise>;
template class LuaClass<DeckPromiseList>;
template class LuaClass<DeckRectangle>;
template class LuaClass<DeckRectangleList>;
template class LuaClass<DeckUtil>;

#ifdef HAVE_VNC
template class LuaClass<ConnectorVnc>;
#endif
