#ifndef DECK_ASSISTANT_CONNECTOR_BASE_H
#define DECK_ASSISTANT_CONNECTOR_BASE_H

#include "lua_class.h"

template <typename T>
class ConnectorBase : public LuaClass<T>
{
public:
	using Super = ConnectorBase<T>;

	virtual void tick_inputs(lua_State* L, lua_Integer clock) = 0;
	virtual void tick_outputs(lua_State* L)                   = 0;
	virtual void shutdown(lua_State* L)                       = 0;

	static bool const LUA_ENABLE_PUSH_THIS = true;
	static void init_class_table(lua_State* L);

protected:
	static int _lua_tick_inputs(lua_State* L);
	static int _lua_tick_outputs(lua_State* L);
	static int _lua_shutdown(lua_State* L);
};

#endif
