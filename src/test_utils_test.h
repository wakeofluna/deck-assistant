#ifndef DECK_ASSISTANT_TEST_UTILS_TEST_H
#define DECK_ASSISTANT_TEST_UTILS_TEST_H

#include <iosfwd>
#include <lua.hpp>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

lua_State* new_test_state();
int at_panic(lua_State* L);

int push_dummy_value(lua_State* L, int tp);

using TableCountMap = std::map<int, std::pair<int, int>>;
TableCountMap count_elements_in_table(lua_State* L, int idx);

std::string_view to_string_view(lua_State* L, int idx);
std::vector<std::string_view> split_string(std::string_view const& text, char split_char = '\n');

#endif // DECK_ASSISTANT_TEST_UTILS_TEST_H
