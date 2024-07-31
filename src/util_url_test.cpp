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

#include "util_url.h"
#include <catch2/catch_test_macros.hpp>

using namespace util;

TEST_CASE("URL", "[util]")
{
	URL url;

	SECTION("Blank URL is blank")
	{
		REQUIRE(url.get_connection_string() == "");
		REQUIRE(url.get_schema() == "");
		REQUIRE(url.get_host() == "");
		REQUIRE(url.get_port() == 0);
		REQUIRE(url.get_path() == "");
	}

	SECTION("Setting a schema on a blank URL")
	{
		REQUIRE(url.set_schema("ws"));
		REQUIRE(url.get_connection_string() == "ws:///");
		REQUIRE(url.get_schema() == "ws");

		REQUIRE(url.set_schema("wss"));
		REQUIRE(url.get_connection_string() == "wss:///");
		REQUIRE(url.get_schema() == "wss");
	}

	SECTION("Setting no schema defaults to https://")
	{
		REQUIRE(url.set_host("local.lan"));
		REQUIRE(url.get_connection_string() == "https://local.lan/");
		REQUIRE(url.get_schema() == "https");
		REQUIRE(url.get_host() == "local.lan");
	}

	SECTION("Setting everything")
	{
		REQUIRE(url.set_schema("https"));
		REQUIRE(url.set_host("compilehost.lan"));
		REQUIRE(url.set_port(8080));
		REQUIRE(url.set_path("index.html"));

		REQUIRE(url.get_connection_string() == "https://compilehost.lan:8080/index.html");
		REQUIRE(url.get_schema() == "https");
		REQUIRE(url.get_host() == "compilehost.lan");
		REQUIRE(url.get_port() == 8080);
		REQUIRE(url.get_path() == "/index.html");
	}

	SECTION("Setting a connection string directly")
	{
		SECTION("All components")
		{
			REQUIRE(url.set_connection_string("http://fake.host:80/self_destruct.php", "foo"));

			REQUIRE(url.get_connection_string() == "http://fake.host:80/self_destruct.php");
			REQUIRE(url.get_schema() == "http");
			REQUIRE(url.get_host() == "fake.host");
			REQUIRE(url.get_port() == 80);
			REQUIRE(url.get_path() == "/self_destruct.php");
		}

		SECTION("Missing schema")
		{
			REQUIRE(url.set_connection_string("fake.host:80/self_destruct.php", "foo"));

			REQUIRE(url.get_connection_string() == "foo://fake.host:80/self_destruct.php");
			REQUIRE(url.get_schema() == "foo");
			REQUIRE(url.get_host() == "fake.host");
			REQUIRE(url.get_port() == 80);
			REQUIRE(url.get_path() == "/self_destruct.php");
		}

		SECTION("Missing host")
		{
			REQUIRE(url.set_connection_string("wss:///self_destruct.php", "foo"));

			REQUIRE(url.get_connection_string() == "wss:///self_destruct.php");
			REQUIRE(url.get_schema() == "wss");
			REQUIRE(url.get_host() == "");
			REQUIRE(url.get_port() == 0);
			REQUIRE(url.get_path() == "/self_destruct.php");
		}

		SECTION("Missing port")
		{
			REQUIRE(url.set_connection_string("http://fake.host/self_destruct.php", "foo"));

			REQUIRE(url.get_connection_string() == "http://fake.host/self_destruct.php");
			REQUIRE(url.get_schema() == "http");
			REQUIRE(url.get_host() == "fake.host");
			REQUIRE(url.get_port() == 0);
			REQUIRE(url.get_path() == "/self_destruct.php");
		}

		SECTION("Missing path")
		{
			REQUIRE(url.set_connection_string("http://fake.host:80", "foo"));

			REQUIRE(url.get_connection_string() == "http://fake.host:80/");
			REQUIRE(url.get_schema() == "http");
			REQUIRE(url.get_host() == "fake.host");
			REQUIRE(url.get_port() == 80);
			REQUIRE(url.get_path() == "/");
		}
	}
}
