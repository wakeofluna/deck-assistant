#include "lua_class.hpp"
#include "connector_elgato_streamdeck.h"
#include "connector_websocket.h"
#include "connector_window.h"
#include "deck_card.h"
#include "deck_colour.h"
#include "deck_connector_container.h"
#include "deck_connector_factory.h"
#include "deck_font.h"
#include "deck_logger.h"
#include "deck_module.h"
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
template class LuaClass<DeckRectangle>;
template class LuaClass<DeckRectangleList>;
template class LuaClass<DeckUtil>;
