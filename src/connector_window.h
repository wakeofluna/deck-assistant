#ifndef DECK_ASSISTANT_CONNECTOR_WINDOW_H
#define DECK_ASSISTANT_CONNECTOR_WINDOW_H

#include "i_connector.h"
#include <SDL.h>
#include <optional>
#include <string>

class ConnectorWindow : public IConnector
{
public:
	ConnectorWindow();
	~ConnectorWindow();

	static char const* SUBTYPE_NAME;
	char const* get_subtype_name() const override;

	void tick(lua_State* L, int delta_msec) override;
	void shutdown(lua_State* L) override;

	void init_instance_table(lua_State* L) override;
	int index(lua_State* L) const override;
	int newindex(lua_State* L) override;

private:
	SDL_Window* m_window;
	std::optional<std::string> m_wanted_title;
	std::optional<int> m_wanted_width;
	std::optional<int> m_wanted_height;
	std::optional<bool> m_wanted_visible;
};

#endif // DECK_ASSISTANT_CONNECTOR_WINDOW_H
