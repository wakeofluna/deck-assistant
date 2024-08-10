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

#include "application.h"
#include <cstdlib>
#include <string_view>
#include <vector>

int submain(std::vector<std::string_view>&& args);

int main(int argc, char** argv)
{
	std::vector<std::string_view> args { argv, argv + argc };

	Application app;

	if (!app.init(std::move(args)))
		return EXIT_FAILURE;

	return app.run();
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// do not reorder
#include <codecvt>
#include <locale>
#include <shellapi.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	int num_args     = 0;
	LPWSTR* win_args = CommandLineToArgvW(GetCommandLineW(), &num_args);

	std::vector<std::string> char_args;
	std::vector<char const*> main_args;
	char_args.reserve(num_args);
	main_args.reserve(num_args + 1);

	std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
	for (int idx = 0; idx < num_args; ++idx)
	{
		char_args.push_back(convert.to_bytes(win_args[idx]));
		main_args.push_back(char_args.back().c_str());
	}

	LocalFree(win_args);

	main_args.push_back(nullptr);
	return main(num_args, const_cast<char**>(main_args.data()));
}
#endif
