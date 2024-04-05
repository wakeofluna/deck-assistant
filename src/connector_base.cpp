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
