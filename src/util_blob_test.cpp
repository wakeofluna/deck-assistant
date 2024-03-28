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
		BlobView blob2("only one!");
		REQUIRE(blob2.size() == 9);
		REQUIRE(blob2.to_bin() == "only one!");
	}

	SECTION("Blob from/to hex")
	{
		std::string_view input = "59657420616e6f746865722068657820696d706c656d656e746174696f6e";

		Blob blob = Blob::from_hex(input);
		REQUIRE(blob.to_bin() == "Yet another hex implementation");

		std::string output = blob.to_hex();
		REQUIRE(output == input);
	}

	SECTION("Blob from/to base64")
	{
		std::string_view input = "WWV0IGFub3RoZXIgYmFzZTY0IGltcGxlbWVudGF0aW9uIQ==";

		Blob blob = Blob::from_base64(input);
		REQUIRE(blob.to_bin() == "Yet another base64 implementation!");

		std::string output = blob.to_base64();
		REQUIRE(output == input);
	}

	SECTION("Blob sha1")
	{
		// Websocket RFC example

		std::string_view websocket_nonce  = "the sample nonce";
		std::string_view websocket_uuid   = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		std::string_view websocket_accept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

		Blob blob(60);
		blob += BlobView(websocket_nonce).to_base64();
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
}
