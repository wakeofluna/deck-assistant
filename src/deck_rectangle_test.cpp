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

#include "deck_rectangle.h"
#include "lua_helpers.h"
#include "test_utils_test.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

TEST_CASE("DeckRectangle", "[deck]")
{
	lua_State* L = new_test_state();

	SECTION("Constructor")
	{
		DeckRectangle* rect1 = DeckRectangle::push_new(L);
		REQUIRE(rect1->get_rectangle().x == 0);
		REQUIRE(rect1->get_rectangle().y == 0);
		REQUIRE(rect1->get_rectangle().w == 0);
		REQUIRE(rect1->get_rectangle().h == 0);

		DeckRectangle* rect2 = DeckRectangle::push_new(L, 44, 55);
		REQUIRE(rect2->get_rectangle().x == 0);
		REQUIRE(rect2->get_rectangle().y == 0);
		REQUIRE(rect2->get_rectangle().w == 44);
		REQUIRE(rect2->get_rectangle().h == 55);

		DeckRectangle* rect3 = DeckRectangle::push_new(L, 10, 20, 30, 40);
		REQUIRE(rect3->get_rectangle().x == 10);
		REQUIRE(rect3->get_rectangle().y == 20);
		REQUIRE(rect3->get_rectangle().w == 30);
		REQUIRE(rect3->get_rectangle().h == 40);
	}

	SECTION("index")
	{
		DeckRectangle* rect1 = DeckRectangle::push_new(L, 10, 20, 30, 40);

		lua_getfield(L, 1, "x");
		REQUIRE(to_int(L, -1) == 10);

		lua_getfield(L, 1, "y");
		REQUIRE(to_int(L, -1) == 20);

		lua_getfield(L, 1, "w");
		REQUIRE(to_int(L, -1) == 30);

		lua_getfield(L, 1, "h");
		REQUIRE(to_int(L, -1) == 40);

		lua_getfield(L, 1, "left");
		REQUIRE(to_int(L, -1) == 10);

		lua_getfield(L, 1, "top");
		REQUIRE(to_int(L, -1) == 20);

		lua_getfield(L, 1, "right");
		REQUIRE(to_int(L, -1) == 40);

		lua_getfield(L, 1, "bottom");
		REQUIRE(to_int(L, -1) == 60);

		lua_getfield(L, 1, "dup");
		DeckRectangle* rect2 = DeckRectangle::from_stack(L, -1, false);
		REQUIRE(rect2 != nullptr);
		REQUIRE(!lua_rawequal(L, 1, -1));

		REQUIRE(rect1 != rect2);
		REQUIRE(rect2->get_rectangle().x == rect1->get_rectangle().x);
		REQUIRE(rect2->get_rectangle().y == rect1->get_rectangle().y);
		REQUIRE(rect2->get_rectangle().w == rect1->get_rectangle().w);
		REQUIRE(rect2->get_rectangle().h == rect1->get_rectangle().h);
	}

	SECTION("newindex")
	{
		DeckRectangle* rect = DeckRectangle::push_new(L);

		lua_pushinteger(L, 100);
		lua_setfield(L, 1, "x");

		lua_pushinteger(L, 200);
		lua_setfield(L, 1, "y");

		lua_pushinteger(L, 300);
		lua_setfield(L, 1, "w");

		lua_pushinteger(L, 400);
		lua_setfield(L, 1, "h");

		REQUIRE(rect->get_rectangle().x == 100);
		REQUIRE(rect->get_rectangle().y == 200);
		REQUIRE(rect->get_rectangle().w == 300);
		REQUIRE(rect->get_rectangle().h == 400);

		SECTION("Unhappy values for width and height")
		{
			lua_pushinteger(L, -1);
			EXPECT_ERROR(lua_setfield(L, 1, "width"));
			REQUIRE_THAT(g_panic_msg, Catch::Matchers::StartsWith("bad argument"));

			lua_pushinteger(L, -80);
			EXPECT_ERROR(lua_setfield(L, 1, "height"));
			REQUIRE_THAT(g_panic_msg, Catch::Matchers::StartsWith("bad argument"));
		}

		SECTION("Happy values for bottom and right")
		{
			lua_pushinteger(L, 150);
			lua_setfield(L, 1, "right");

			lua_pushinteger(L, 350);
			lua_setfield(L, 1, "bottom");

			REQUIRE(rect->get_rectangle().x == 100);
			REQUIRE(rect->get_rectangle().y == 200);
			REQUIRE(rect->get_rectangle().w == 50);
			REQUIRE(rect->get_rectangle().h == 150);
		}

		SECTION("Unhappy values for bottom and right")
		{
			lua_pushinteger(L, 99);
			EXPECT_ERROR(lua_setfield(L, 1, "right"));
			REQUIRE_THAT(g_panic_msg, Catch::Matchers::StartsWith("bad argument"));

			lua_pushinteger(L, 199);
			EXPECT_ERROR(lua_setfield(L, 1, "bottom"));
			REQUIRE_THAT(g_panic_msg, Catch::Matchers::StartsWith("bad argument"));
		}
	}

	SECTION("clip")
	{
		SDL_Rect lhs {};
		SDL_Rect rhs {};
		SDL_Rect clip {};

		SECTION("Identical rectangles")
		{
			lhs  = SDL_Rect { 40, 50, 200, 300 };
			rhs  = lhs;
			clip = DeckRectangle::clip(lhs, rhs);
			REQUIRE(clip.x == lhs.x);
			REQUIRE(clip.y == lhs.y);
			REQUIRE(clip.w == lhs.w);
			REQUIRE(clip.h == lhs.h);
		}

		SECTION("Oversized on x-axis")
		{
			lhs  = SDL_Rect { -25, 75, 50, 50 };
			rhs  = SDL_Rect { -50, 80, 100, 10 };
			clip = DeckRectangle::clip(lhs, rhs);
			REQUIRE(clip.x == lhs.x);
			REQUIRE(clip.y == rhs.y);
			REQUIRE(clip.w == lhs.w);
			REQUIRE(clip.h == rhs.h);
		}

		SECTION("Oversized on y-axis")
		{
			lhs  = SDL_Rect { 25, 75, 50, 50 };
			rhs  = SDL_Rect { 35, 40, 10, 100 };
			clip = DeckRectangle::clip(lhs, rhs);
			REQUIRE(clip.x == rhs.x);
			REQUIRE(clip.y == lhs.y);
			REQUIRE(clip.w == rhs.w);
			REQUIRE(clip.h == lhs.h);
		}

		SECTION("Corner overlap")
		{
			lhs  = SDL_Rect { 25, 35, 55, 70 };
			rhs  = SDL_Rect { 50, 60, 60, 70 };
			clip = DeckRectangle::clip(lhs, rhs);
			REQUIRE(clip.x == rhs.x);
			REQUIRE(clip.y == rhs.y);
			REQUIRE(clip.w == 30);
			REQUIRE(clip.h == 45);
		}

		SECTION("No overlap on x-axis")
		{
			lhs  = SDL_Rect { 25, 40, 55, 70 };
			rhs  = SDL_Rect { 100, 50, 60, 20 };
			clip = DeckRectangle::clip(lhs, rhs);
			REQUIRE(clip.x == lhs.x + lhs.w);
			REQUIRE(clip.y == rhs.y);
			REQUIRE(clip.w == 0);
			REQUIRE(clip.h == rhs.h);
		}

		SECTION("No overlap on y-axis")
		{
			lhs  = SDL_Rect { 20, 40, 60, 30 };
			rhs  = SDL_Rect { 30, 80, 50, 20 };
			clip = DeckRectangle::clip(lhs, rhs);
			REQUIRE(clip.x == rhs.x);
			REQUIRE(clip.y == lhs.y + lhs.h);
			REQUIRE(clip.w == rhs.w);
			REQUIRE(clip.h == 0);
		}

		SECTION("No overlap on either axis")
		{
			lhs  = SDL_Rect { 20, 30, 40, 50 };
			rhs  = SDL_Rect { 120, 130, 140, 150 };
			clip = DeckRectangle::clip(lhs, rhs);
			REQUIRE(clip.x == lhs.x + lhs.w);
			REQUIRE(clip.y == lhs.y + lhs.h);
			REQUIRE(clip.w == 0);
			REQUIRE(clip.h == 0);
		}

		// Check that clip with inverted arguments produces the same results for all cases
		SDL_Rect clip2 = DeckRectangle::clip(rhs, lhs);
		REQUIRE(clip2.w == clip.w);
		REQUIRE(clip2.h == clip.h);
		if (clip2.w != 0)
			REQUIRE(clip2.x == clip.x);
		else
			REQUIRE(clip2.x == rhs.x);

		if (clip2.h != 0)
			REQUIRE(clip2.y == clip.y);
		else
			REQUIRE(clip2.y == rhs.y);
	}

	SECTION("contains")
	{
		DeckRectangle::push_new(L, 10, 20, 30, 40);

		auto contains = [L](int x, int y) -> bool {
			lua_getfield(L, 1, "contains");
			lua_pushvalue(L, 1);
			lua_pushinteger(L, x);
			lua_pushinteger(L, y);

			if (lua_pcall(L, 3, 1, 0) != LUA_OK)
				FAIL(LuaHelpers::to_string_view(L, -1));

			bool result = lua_toboolean(L, -1);
			lua_pop(L, 1);
			return result;
		};

		REQUIRE(contains(0, 0) == false);
		REQUIRE(contains(100, 0) == false);
		REQUIRE(contains(0, 100) == false);
		REQUIRE(contains(100, 100) == false);

		REQUIRE(contains(10, 20) == true);
		REQUIRE(contains(39, 20) == true);
		REQUIRE(contains(10, 59) == true);
		REQUIRE(contains(39, 59) == true);

		REQUIRE(contains(25, 35) == true);

		REQUIRE(contains(9, 20) == false);
		REQUIRE(contains(40, 20) == false);
		REQUIRE(contains(9, 59) == false);
		REQUIRE(contains(40, 59) == false);

		REQUIRE(contains(10, 19) == false);
		REQUIRE(contains(39, 19) == false);
		REQUIRE(contains(10, 60) == false);
		REQUIRE(contains(39, 60) == false);
	}

	lua_close(L);
}
