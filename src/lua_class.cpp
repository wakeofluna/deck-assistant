#include "lua_class.hpp"
#include "deck_card.h"
#include "deck_colour.h"
#include "deck_connector.h"
#include "deck_connector_container.h"
#include "deck_font.h"
#include "deck_logger.h"
#include "deck_module.h"
#include "deck_rectangle.h"

template class LuaClass<DeckCard>;
template class LuaClass<DeckColour>;
template class LuaClass<DeckConnectorContainer>;
template class LuaClass<DeckConnector>;
template class LuaClass<DeckFont>;
template class LuaClass<DeckLogger>;
template class LuaClass<DeckModule>;
template class LuaClass<DeckRectangle>;
