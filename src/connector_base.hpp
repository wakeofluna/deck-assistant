#ifndef DECK_ASSISTANT_CONNECTOR_BASE_HPP
#define DECK_ASSISTANT_CONNECTOR_BASE_HPP

#include "connector_base.h"
#include "lua_helpers.h"

template <typename T>
void ConnectorBase<T>::init_class_table(lua_State* L)
{
	lua_pushcfunction(L, &_lua_tick_inputs);
	lua_setfield(L, -2, "tick_inputs");

	lua_pushcfunction(L, &_lua_tick_outputs);
	lua_setfield(L, -2, "tick_outputs");

	lua_pushcfunction(L, &_lua_shutdown);
	lua_setfield(L, -2, "shutdown");
}

template <typename T>
void ConnectorBase<T>::finalize(lua_State* L)
{
	shutdown(L);
}

template <typename T>
int ConnectorBase<T>::_lua_tick_inputs(lua_State* L)
{
	T* self           = LuaClass<T>::from_stack(L, 1);
	lua_Integer clock = LuaHelpers::check_arg_int(L, 2);

	self->tick_inputs(L, clock);
	return 0;
}

template <typename T>
int ConnectorBase<T>::_lua_tick_outputs(lua_State* L)
{
	T* self = LuaClass<T>::from_stack(L, 1);
	self->tick_outputs(L);
	return 0;
}

template <typename T>
int ConnectorBase<T>::_lua_shutdown(lua_State* L)
{
	T* self = LuaClass<T>::from_stack(L, 1);
	self->shutdown(L);
	return 0;
}

#endif // DECK_ASSISTANT_CONNECTOR_BASE_HPP
