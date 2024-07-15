/*
 * DeckAssistant - Creating control panels using scripts.
 * Copyright (C) 2024  Esther Dalhuisen (Wake of Luna)
 *
 * DeckAssistant is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DeckAssistant is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "lua_helpers.h"
#include "test_utils_test.h"
#include "util_text.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <tuple>
#include <vector>

namespace
{

void push_json_test_table(lua_State* L, bool add_ignored_things)
{
	lua_createtable(L, 0, 10);

	lua_pushboolean(L, true);
	lua_setfield(L, -2, "bool_true");
	lua_pushboolean(L, false);
	lua_setfield(L, -2, "bool_false");
	lua_pushinteger(L, 1234);
	lua_setfield(L, -2, "some_int");
	lua_pushnumber(L, 42.069);
	lua_setfield(L, -2, "some_float");
	lua_pushliteral(L, "show me");
	lua_setfield(L, -2, "some string");

	if (add_ignored_things)
	{
		lua_pushcfunction(L, &at_panic);
		lua_setfield(L, -2, "some_func");
		lua_newthread(L);
		lua_setfield(L, -2, "some_state");
	}

	lua_createtable(L, 0, 2);
	if (add_ignored_things)
	{
		lua_pushinteger(L, 12);
		lua_pushliteral(L, "index 12");
		lua_rawset(L, -3);
	}
	lua_pushliteral(L, "1");
	lua_pushliteral(L, "index 1");
	lua_rawset(L, -3);
	lua_setfield(L, -2, "some_obj");

	lua_createtable(L, 3, 0);
	lua_pushliteral(L, "value1");
	lua_rawseti(L, -2, 1);
	lua_pushliteral(L, "value2");
	lua_rawseti(L, -2, 2);
	lua_pushnumber(L, 2024);
	lua_rawseti(L, -2, 3);
	lua_setfield(L, -2, "some_arr");
}

void verify_json_test_table(lua_State* L, int idx = -1)
{
	REQUIRE(lua_istable(L, idx));

	while (get_and_pop_key_value_in_table(L, idx))
	{
		std::string_view key = LuaHelpers::to_string_view(L, -2);
		if (key == "bool_true")
		{
			REQUIRE(lua_toboolean(L, -1) == true);
		}
		else if (key == "bool_false")
		{
			REQUIRE(lua_toboolean(L, -1) == false);
		}
		else if (key == "some_int")
		{
			REQUIRE(lua_tointeger(L, -1) == 1234);
		}
		else if (key == "some_float")
		{
			REQUIRE(lua_tonumber(L, -1) == 42.069);
		}
		else if (key == "some string")
		{
			std::string_view value = LuaHelpers::to_string_view(L, -1);
			REQUIRE(value == "show me");
		}
		else if (key == "some_obj")
		{
			REQUIRE(lua_istable(L, -1));
			lua_getfield(L, -1, "1");
			REQUIRE(lua_tostring(L, -1) == std::string_view("index 1"));
			lua_pop(L, 1);
		}
		else if (key == "some_arr")
		{
			REQUIRE(lua_istable(L, -1));
			lua_rawgeti(L, -1, 1);
			lua_rawgeti(L, -2, 2);
			lua_rawgeti(L, -3, 3);
			lua_rawgeti(L, -4, 4);
			REQUIRE(lua_tostring(L, -4) == std::string_view("value1"));
			REQUIRE(lua_tostring(L, -3) == std::string_view("value2"));
			REQUIRE(lua_tointeger(L, -2) == 2024);
			REQUIRE(lua_isnil(L, -1));
			lua_pop(L, 4);
		}
		else if (key.empty())
		{
			FAIL("Non-string key in table");
		}
		else
		{
			FAIL("Unexpected key in table: " << key);
		}
		lua_pop(L, 2);
	}
}

} // namespace

TEST_CASE("Text", "[util]")
{
	SECTION("hex_to_char")
	{
		char const* subject                    = "1234567890abcdefABCDEF";
		std::array<unsigned char, 11> expected = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef };
		std::array<unsigned char, 11> result1, result2;
		bool ok = true;

		for (std::size_t i = 0; i < 11; ++i)
		{
			std::size_t offset = i * 2;
			result1[i]         = util::hex_to_char(subject + offset);
			result2[i]         = util::hex_to_char(subject + offset, ok);
		}

		REQUIRE(result1 == expected);
		REQUIRE(result2 == expected);
		REQUIRE(result1 == expected);
		REQUIRE(result2 == expected);
		REQUIRE(ok);

		REQUIRE(util::hex_to_char("0g") == 0x00);
		REQUIRE(util::hex_to_char("0g", ok) == 0x00);
		REQUIRE_FALSE(ok);
	}

	SECTION("char_to_hex")
	{
		using TestPair = std::tuple<unsigned char, char const*, char const*>;

		std::array<char, 2> buf1, buf2;

		auto [inp, exp1, exp2] = GENERATE(values<TestPair>({
		    {0x00,  "00", "00"},
		    { 0x12, "12", "12"},
		    { 0x9a, "9a", "9A"},
		    { 0xef, "ef", "EF"},
		}));

		std::array<char, 2> expected1 = { exp1[0], exp1[1] };
		std::array<char, 2> expected2 = { exp2[0], exp2[1] };
		util::char_to_hex(inp, buf1.data());
		util::char_to_hex_uc(inp, buf2.data());
		REQUIRE(buf1 == expected1);
		REQUIRE(buf2 == expected2);
	}

	SECTION("convert_to_json")
	{
		lua_State* L = new_test_state();
		push_json_test_table(L, true);

		// Cannot do exact matches because lua table ordering is undefined
		// {"some_obj":{"1":"index 1"},"some_arr":["value1","value2",2024],"bool_true":true,"bool_false":false,"some_int":1234,"some_float":42.069,"some string":"show me"}

		SECTION("No whitespace")
		{
			std::string json = util::convert_to_json(L, -1);
			REQUIRE_THAT(json, Catch::Matchers::StartsWith("{"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\"some_obj\":{\"1\":\"index 1\"}"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\"some_arr\":[\"value1\",\"value2\",2024]"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\"bool_true\":true"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\"bool_false\":false"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\"some_int\":1234"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\"some_float\":42.069"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\"some string\":\"show me\""));
			REQUIRE_THAT(json, Catch::Matchers::EndsWith("}"));
		}

		SECTION("Whitespace")
		{
			std::string json = util::convert_to_json(L, -1, true);
			REQUIRE_THAT(json, Catch::Matchers::StartsWith("{\n"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\n  \"some_obj\": {\n    \"1\": \"index 1\"\n  }"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\n  \"some_arr\": [\n    \"value1\",\n    \"value2\",\n    2024\n  ]"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\n  \"bool_true\": true"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\n  \"bool_false\": false"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\n  \"some_int\": 1234"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\n  \"some_float\": 42.069"));
			REQUIRE_THAT(json, Catch::Matchers::ContainsSubstring("\n  \"some string\": \"show me\""));
			REQUIRE_THAT(json, Catch::Matchers::EndsWith("\n}"));
		}
	}

	SECTION("convert_from_json")
	{
		SECTION("No errors, no whitespace")
		{
			std::string_view const input = R"json({"some_obj":{"1":"index 1"},"some_arr":["value1","value2",2024],"bool_true":true,"bool_false":false,"some_int":1234,"some_float":42.069,"some string":"show me"})json";

			lua_State* L = new_test_state();

			std::size_t offset   = 0;
			std::string_view err = util::convert_from_json(L, input, offset);

			REQUIRE(err == "");
			REQUIRE(offset == input.size());

			REQUIRE(lua_gettop(L) == 1);
			verify_json_test_table(L);
		}

		SECTION("No errors, some whitespace")
		{
			std::string_view const input = R"json(\n{\n  "some_obj" : \n{ \n"1"\n: \t\t "index 1"  \t}\n,"some_arr":["value1","value2",2024],"bool_true":true, "bool_false": false,\n  "some_int": 1234\n,  "some_float"\n:\t42.069,"some string":  "show me"})json";

			lua_State* L = new_test_state();

			std::string json = util::replace(input, "\\n", "\n");
			json             = util::replace(json, "\\t", "\t");

			std::size_t offset   = 0;
			std::string_view err = util::convert_from_json(L, json, offset);

			REQUIRE(err == "");
			REQUIRE(offset == json.size());

			REQUIRE(lua_gettop(L) == 1);
			verify_json_test_table(L);
		}

		SECTION("Syntax errors")
		{
			// The error is an extra comma behind the last element of some_obj
			std::string_view const input = R"json(\n{\n  "some_obj" : \n{ \n"1"\n: \t\t "index 1"  ,\t}\n,"some_arr":["value1","value2",2024],"bool_true":true, "bool_false": false,\n  "some_int": 1234\n,  "some_float"\n:\t42.069,"some string":  "show me"})json";

			lua_State* L = new_test_state();

			std::string json = util::replace(input, "\\n", "\n");
			json             = util::replace(json, "\\t", "\t");

			std::size_t offset   = 0;
			std::string_view err = util::convert_from_json(L, json, offset);

			// Error is positioned at the } after the comma, since we expect a string instead of a closing brace
			REQUIRE_FALSE(err.empty());
			REQUIRE(offset < json.size());
			REQUIRE(json[offset] == '}');

			REQUIRE(lua_gettop(L) == 0);
		}
	}

	SECTION("load_file")
	{
		SUCCEED("Skipping tests that access the live filesystem");
	}

	SECTION("trim")
	{
		using TestPair = std::pair<std::string_view, std::string_view>;

		auto [input, expected] = GENERATE(values<TestPair>({
		    {"",		               ""               },
		    { "    ",                  ""               },
		    { "    abcd    ",          "abcd"           },
		    { "\t  { some things }\n", "{ some things }"},
		}));

		std::string_view trimmed = util::trim(input);
		REQUIRE(trimmed == expected);
	}

	SECTION("split")
	{
		using Result = std::vector<std::string_view>;
		Result result;

		result = util::split("line1\nline2\nline3\nline4\n");
		REQUIRE(result == Result { "line1", "line2", "line3", "line4", "" });

		result = util::split("line1\nline2\nline3\nline4");
		REQUIRE(result == Result { "line1", "line2", "line3", "line4" });

		result = util::split("line1\nline2\nline3\nline4\n", "\n", 3);
		REQUIRE(result == Result { "line1", "line2", "line3\nline4\n" });

		result = util::split("line1\\nline2\\nline3\\nline4\\n", "\\n");
		REQUIRE(result == Result { "line1", "line2", "line3", "line4", "" });

		result = util::split("env=PATH=/usr/bin", "=", 2);
		REQUIRE(result == Result { "env", "PATH=/usr/bin" });
	}

	SECTION("split1")
	{
		using Result = std::pair<std::string_view, std::string_view>;
		Result result;

		result = util::split1("env=PATH=/usr/bin", "=");
		REQUIRE(result == Result { "env", "PATH=/usr/bin" });

		result = util::split1("line1\nline2\nline3\nline4\n", "\n");
		REQUIRE(result == Result { "line1", "line2\nline3\nline4" });

		result = util::split1("line1\nline2\nline3\nline4\n", "\n", false);
		REQUIRE(result == Result { "line1", "line2\nline3\nline4\n" });

		result = util::split1("setting = important value\n", "=");
		REQUIRE(result == Result { "setting", "important value" });

		result = util::split1("setting = important value\n", "=", false);
		REQUIRE(result == Result { "setting ", " important value\n" });

		result = util::split1("= empty\n", "=");
		REQUIRE(result == Result { "", "empty" });

		result = util::split1("no value\n", "=");
		REQUIRE(result == Result { "no value", "" });
	}

	SECTION("join")
	{
		using TestPair = std::tuple<std::string_view, std::string_view>;

		std::vector<std::string_view> input = util::split("line1\nline2\nline3\nline4");
		REQUIRE(input.size() == 4);

		auto [join_str, expected] = GENERATE(values<TestPair>({
		    {";",    "line1;line2;line3;line4"      },
		    { "\n",  "line1\nline2\nline3\nline4"   },
		    { "\t ", "line1\t line2\t line3\t line4"},
		    { "",    "line1line2line3line4"         },
		}));

		std::string result = util::join(input, join_str);
		REQUIRE(result == expected);
	}

	SECTION("replace")
	{
		using TestPair = std::tuple<std::string_view, std::string_view>;

		std::string_view input = "line1\nline2\nline3\nline4";

		auto [join_str, expected] = GENERATE(values<TestPair>({
		    {";",    "line1;line2;line3;line4"      },
		    { "\n",  "line1\nline2\nline3\nline4"   },
		    { "\t ", "line1\t line2\t line3\t line4"},
		    { "",    "line1line2line3line4"         },
		}));

		std::string result = util::replace(input, "\n", join_str);
		REQUIRE(result == expected);
	}

	SECTION("for_each_split")
	{
		std::string_view result;
		std::size_t remainder;

		using Result = std::vector<std::pair<std::size_t, std::string_view>>;
		Result collected;

		auto collect = [&](std::size_t part_nr, std::string_view const& part) {
			collected.emplace_back(part_nr, part);
			return part_nr == 5;
		};

		collected.clear();
		std::tie(result, remainder) = util::for_each_split("line1\nline2\nline3\nline4\n", "\n", collect);
		REQUIRE(result.empty());
		REQUIRE(remainder == 24);
		REQUIRE(collected == Result {
		            {0,  "line1"},
		            { 1, "line2"},
		            { 2, "line3"},
		            { 3, "line4"},
		            { 4, ""     },
        });

		collected.clear();
		std::tie(result, remainder) = util::for_each_split("line1;line2;line3\nline3b;line4;line5;line6;line7;line8", ";", collect);
		REQUIRE(result == "line6");
		REQUIRE(remainder == 43);
		REQUIRE(collected == Result {
		            {0,  "line1"        },
		            { 1, "line2"        },
		            { 2, "line3\nline3b"},
		            { 3, "line4"        },
		            { 4, "line5"        },
		            { 5, "line6"        },
        });
	}

	SECTION("parse_http_message")
	{
		util::HttpMessage http_message;

		std::string response_start_line = "HTTP/1.1 404 Not Found\r\n";
		std::string request_start_line  = "GET /foo?bar=true HTTP/1.1\r\n";
		std::string request_headers[]   = {
            "Host: localhost\r\n",
            "Accept: text/html\r\n",
		};
		std::string response_headers[] = {
			"Server: catch2\r\n",
			"Content-Type: text/html\r\n",
		};
		std::string headers_end = "\r\n";
		std::string body        = "SOME BODY DATA\r\n";

		SECTION("Good request with body")
		{
			std::string request = request_start_line + request_headers[0] + request_headers[1] + headers_end + body;
			http_message        = util::parse_http_message(request);

			REQUIRE(http_message);
			REQUIRE(http_message.error == "");
			REQUIRE(http_message.request_method == "GET");
			REQUIRE(http_message.request_path == "/foo?bar=true");
			REQUIRE(http_message.http_version == "HTTP/1.1");
			REQUIRE(http_message.response_status_code == 0);
			REQUIRE(http_message.response_status_message == "");
			REQUIRE(http_message.headers.size() == 2);
			REQUIRE(http_message.headers["Host"] == "localhost");
			REQUIRE(http_message.headers["Accept"] == "text/html");

			std::string_view body = std::string_view(request).substr(http_message.body_start);
			REQUIRE(body == "SOME BODY DATA\r\n");
		}

		SECTION("Good request without body")
		{
			std::string request = request_start_line + request_headers[0] + request_headers[1] + headers_end;
			http_message        = util::parse_http_message(request);

			REQUIRE(http_message);
			REQUIRE(http_message.error == "");
			REQUIRE(http_message.request_method == "GET");
			REQUIRE(http_message.request_path == "/foo?bar=true");
			REQUIRE(http_message.http_version == "HTTP/1.1");
			REQUIRE(http_message.response_status_code == 0);
			REQUIRE(http_message.response_status_message == "");
			REQUIRE(http_message.headers.size() == 2);
			REQUIRE(http_message.headers["Host"] == "localhost");
			REQUIRE(http_message.headers["Accept"] == "text/html");

			std::string_view body = std::string_view(request).substr(http_message.body_start);
			REQUIRE(body == "");
		}

		SECTION("Incomplete request")
		{
			std::string request = request_start_line + request_headers[0] + request_headers[1];
			http_message        = util::parse_http_message(request);

			REQUIRE_FALSE(http_message);
			REQUIRE(http_message.error == "");
			REQUIRE(http_message.request_method == "GET");
			REQUIRE(http_message.request_path == "/foo?bar=true");
			REQUIRE(http_message.http_version == "HTTP/1.1");
			REQUIRE(http_message.response_status_code == 0);
			REQUIRE(http_message.response_status_message == "");
			REQUIRE(http_message.headers.size() == 2);
			REQUIRE(http_message.headers["Host"] == "localhost");
			REQUIRE(http_message.headers["Accept"] == "text/html");
		}

		SECTION("Good response with body")
		{
			std::string request = response_start_line + response_headers[0] + response_headers[1] + headers_end + body;
			http_message        = util::parse_http_message(request);

			REQUIRE(http_message);
			REQUIRE(http_message.error == "");
			REQUIRE(http_message.request_method == "");
			REQUIRE(http_message.request_path == "");
			REQUIRE(http_message.http_version == "HTTP/1.1");
			REQUIRE(http_message.response_status_code == 404);
			REQUIRE(http_message.response_status_message == "Not Found");
			REQUIRE(http_message.headers.size() == 2);
			REQUIRE(http_message.headers["Server"] == "catch2");
			REQUIRE(http_message.headers["Content-Type"] == "text/html");

			std::string_view body = std::string_view(request).substr(http_message.body_start);
			REQUIRE(body == "SOME BODY DATA\r\n");
		}

		SECTION("Good response without body")
		{
			response_start_line = "HTTP/1.1 403 Forbidden\r\n";

			std::string request = response_start_line + response_headers[0] + response_headers[1] + headers_end;
			http_message        = util::parse_http_message(request);

			REQUIRE(http_message);
			REQUIRE(http_message.error == "");
			REQUIRE(http_message.request_method == "");
			REQUIRE(http_message.request_path == "");
			REQUIRE(http_message.http_version == "HTTP/1.1");
			REQUIRE(http_message.response_status_code == 403);
			REQUIRE(http_message.response_status_message == "Forbidden");
			REQUIRE(http_message.headers.size() == 2);
			REQUIRE(http_message.headers["Server"] == "catch2");
			REQUIRE(http_message.headers["Content-Type"] == "text/html");

			std::string_view body = std::string_view(request).substr(http_message.body_start);
			REQUIRE(body == "");
		}

		SECTION("Incomplete response")
		{
			std::string request = response_start_line + response_headers[0] + response_headers[1];
			http_message        = util::parse_http_message(request);

			REQUIRE_FALSE(http_message);
			REQUIRE(http_message.error == "");
			REQUIRE(http_message.request_method == "");
			REQUIRE(http_message.request_path == "");
			REQUIRE(http_message.http_version == "HTTP/1.1");
			REQUIRE(http_message.response_status_code == 404);
			REQUIRE(http_message.response_status_message == "Not Found");
			REQUIRE(http_message.headers.size() == 2);
			REQUIRE(http_message.headers["Server"] == "catch2");
			REQUIRE(http_message.headers["Content-Type"] == "text/html");
		}

		SECTION("Invalid header in response")
		{
			response_headers[1] = "Content Type: text/html";

			std::string request = response_start_line + response_headers[0] + response_headers[1];
			http_message        = util::parse_http_message(request);

			REQUIRE_FALSE(http_message);
			REQUIRE_FALSE(http_message.error.empty());
			REQUIRE(http_message.request_method == "");
			REQUIRE(http_message.request_path == "");
			REQUIRE(http_message.http_version == "HTTP/1.1");
			REQUIRE(http_message.response_status_code == 404);
			REQUIRE(http_message.response_status_message == "Not Found");
			REQUIRE(http_message.headers.size() == 1);
			REQUIRE(http_message.headers["Server"] == "catch2");
		}
	}
}
