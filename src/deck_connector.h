#ifndef DECK_ASSISTANT_DECK_CONNECTOR_H
#define DECK_ASSISTANT_DECK_CONNECTOR_H

#include "i_connector.h"
#include "lua_class.h"
#include <memory>

class DeckConnector : public LuaClass<DeckConnector>
{
public:
	static char const* LUA_TYPENAME;

	DeckConnector(std::unique_ptr<IConnector>&& connector);
	~DeckConnector();

	inline IConnector const* get_connector() const { return m_connector.get(); }
	inline IConnector* get_connector() { return m_connector.get(); }

	void post_init(lua_State* L);
	void tick(lua_State* L, int delta_msec);
	void shutdown(lua_State* L);

	void init_instance_table(lua_State* L);
	int index(lua_State* L) const;
	int newindex(lua_State* L);
	int to_string(lua_State* L) const;

private:
	std::unique_ptr<IConnector> m_connector;
};

#endif // DECK_ASSISTANT_DECK_CONNECTOR_H
