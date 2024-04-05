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

#include "util_blob.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Blob", "[util]")
{
	SECTION("Empty blob")
	{
		Blob blob;
		REQUIRE(blob.size() == 0);
		REQUIRE(blob.capacity() == 0);

		Blob blob2(16);
		REQUIRE(blob2.size() == 0);
		REQUIRE(blob2.capacity() == 16);
	}

	SECTION("Blob from random")
	{
		Blob blob1 = Blob::from_random(4);
		Blob blob2;

		for (std::size_t size = 8; size < 16; ++size)
		{
			for (int i = 0; i < 10; ++i)
			{
				// There is a tiny chance this fails but that's acceptable
				blob2 = Blob::from_random(size);
				REQUIRE(blob2.size() == size);
				REQUIRE(blob1 != blob2);
				blob1 = std::move(blob2);
			}
		}
	}

	SECTION("Blob from literal")
	{
		Blob blob = Blob::from_literal("There can be");
		REQUIRE(blob.size() == 12);
		REQUIRE(blob.to_bin() == "There can be");

		BlobView blob2("only one!");
		REQUIRE(blob2.size() == 9);
		REQUIRE(blob2.to_bin() == "only one!");
	}

	SECTION("Blob hex")
	{
		std::string_view input = "59657420616e6f746865722068657820696d706c656d656e746174696f6e";
		bool ok;

		Blob blob = Blob::from_hex(input, ok);
		REQUIRE(ok);
		REQUIRE(blob.to_bin() == "Yet another hex implementation");

		std::string output = blob.to_hex();
		REQUIRE(output == input);

		input = "0123456789abcdef";
		blob  = Blob::from_hex(input, ok);
		REQUIRE(ok);
		REQUIRE(blob.size() == 8);

		input      = "0123456789ABCDEF";
		Blob blob2 = Blob::from_hex(input, ok);
		REQUIRE(ok);
		REQUIRE(blob2.size() == 8);
		REQUIRE(blob2 == blob);

		input      = "0123456789ABCDEFGH";
		Blob blob3 = Blob::from_hex(input, ok);
		REQUIRE_FALSE(ok);
	}

	SECTION("Blob base64")
	{
		std::string_view input = "WWV0IGFub3RoZXIgYmFzZTY0IGltcGxlbWVudGF0aW9uIQ==";
		bool ok;

		Blob blob = Blob::from_base64(input, ok);
		REQUIRE(ok);
		REQUIRE(blob.to_bin() == "Yet another base64 implementation!");

		std::string output = blob.to_base64();
		REQUIRE(output == input);

		input = "WWV0IGFub3RoZXIgYmFzZTY0!GltcGxlbWVudGF0aW9uIQ==";
		blob  = Blob::from_base64(input, ok);
		REQUIRE_FALSE(ok);
	}

#ifdef HAVE_GNUTLS
	SECTION("Blob sha1")
	{
		// Websocket RFC example

		std::string_view websocket_nonce  = "the sample nonce";
		std::string_view websocket_uuid   = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		std::string_view websocket_accept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

		Blob websocket_key = Blob::from_literal(websocket_nonce);
		Blob blob          = Blob::from_literal(websocket_key.to_base64());
		REQUIRE(blob.to_bin() == "dGhlIHNhbXBsZSBub25jZQ==");

		blob             += websocket_uuid;
		Blob blob_result  = blob.sha1();
		REQUIRE(blob_result.size() == 20);
		REQUIRE(blob_result.to_base64() == websocket_accept);
	}

	SECTION("Blob sha256")
	{
		// OBS auth example

		std::string_view obs_challenge = "+IxH4CnCiqpX1rM9scsNynZzbOe4KhDeYcTNS3PDaeY=";
		std::string_view obs_salt      = "lM1GncleQOaCu9lT1yeUZhFYnqhsLLP1G5lAGo3ixaI=";
		std::string_view obs_password  = "supersecretpassword";
		std::string_view obs_auth      = "1Ct943GAT+6YQUUX47Ia/ncufilbe6+oD6lY+5kaCu4=";

		Blob blob1(80);
		blob1            += obs_password;
		blob1            += obs_salt;
		Blob blob_secret  = blob1.sha256();

		Blob blob2(80);
		blob2 += blob_secret.to_base64();
		blob2 += obs_challenge;

		Blob auth = blob2.sha256();

		REQUIRE(auth.size() == 32);
		REQUIRE(auth.to_base64() == obs_auth);
	}
#endif
}
