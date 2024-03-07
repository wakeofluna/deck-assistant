#ifndef DECK_ASSISTANT_I_CONNECTOR_H
#define DECK_ASSISTANT_I_CONNECTOR_H

#include "lua_class.h"
#include <cstdint>

struct IConnector
{
	IConnector();
	virtual ~IConnector();

	virtual char const* get_subtype_name() const = 0;

	virtual void post_init(lua_State* L);
	virtual void tick(lua_State* L, int delta_msec) = 0;
	virtual void shutdown(lua_State* L);

	virtual void init_instance_table(lua_State* L);
	virtual int index(lua_State* L) const = 0;
	virtual int newindex(lua_State* L)    = 0;
};

#endif // DECK_ASSISTANT_I_CONNECTOR_H
