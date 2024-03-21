#ifndef DECK_ASSISTANT_CONNECTOR_WINDOW_H
#define DECK_ASSISTANT_CONNECTOR_WINDOW_H

#include "connector_base.h"
#include <SDL.h>
#include <atomic>
#include <optional>
#include <string>

class DeckCard;

class ConnectorWindow : public ConnectorBase<ConnectorWindow>
{
public:
	ConnectorWindow();
	~ConnectorWindow();

	void tick_inputs(lua_State* L, lua_Integer clock) override;
	void tick_outputs(lua_State* L) override;
	void shutdown(lua_State* L) override;

	static char const* LUA_TYPENAME;
	static void init_class_table(lua_State* L);
	void init_instance_table(lua_State* L);
	int index(lua_State* L, std::string_view const& key) const;
	int newindex(lua_State* L, std::string_view const& key);

private:
	bool attempt_create_window(lua_State* L);
	void emit_event(lua_State* L, char const* func_name);

	static int _sdl_event_filter(void* userdata, SDL_Event* event);

private:
	// Window physicals
	SDL_Window* m_window;
	std::optional<std::string> m_wanted_title;
	std::optional<int> m_wanted_width;
	std::optional<int> m_wanted_height;
	std::optional<bool> m_wanted_visible;

	// Event signalling flags - naming is a bit funky because of how they work
	std::atomic_flag m_window_size_is_ok;
	std::atomic_flag m_window_surface_is_ok;

	// Deck-related
	DeckCard* m_card;
};

#endif // DECK_ASSISTANT_CONNECTOR_WINDOW_H
