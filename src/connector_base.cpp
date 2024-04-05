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

#include "connector_base.hpp"
#include "connector_elgato_streamdeck.h"
#include "connector_vnc.h"
#include "connector_websocket.h"
#include "connector_window.h"

template class ConnectorBase<ConnectorElgatoStreamDeck>;
template class ConnectorBase<ConnectorWebsocket>;
template class ConnectorBase<ConnectorWindow>;

#ifdef HAVE_VNC
template class ConnectorBase<ConnectorVnc>;
#endif
