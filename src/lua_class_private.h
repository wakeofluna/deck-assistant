#ifndef DECK_ASSISTANT_LUA_CLASS_PRIVATE_H_
#define DECK_ASSISTANT_LUA_CLASS_PRIVATE_H_

#include <lua.hpp>

// Indices in class metatable
constexpr lua_Integer IDX_META_CLASSTABLE = 1;

constexpr char const k_instance_list_key[]   = "deck-assistant.Instances";
constexpr char const k_instance_tables_key[] = "deck-assistant.InstanceTables";

#endif // DECK_ASSISTANT_LUA_CLASS_PRIVATE_H_
