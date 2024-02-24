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
