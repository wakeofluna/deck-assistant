#ifndef DECK_ASSISTANT_APPLICATION_H
#define DECK_ASSISTANT_APPLICATION_H

#include <cstdint>
#include <string_view>
#include <vector>

struct ApplicationPrivate;

class Application
{
public:
	Application();
	Application(Application const&) = delete;
	Application(Application&&);
	~Application();

	Application& operator=(Application const&) = delete;
	Application& operator=(Application&&);

	bool init(std::vector<std::string_view>&& args);
	int run();

private:
	void install_function_overrides();

private:
	ApplicationPrivate* m_private;
};

#endif // DECK_ASSISTANT_APPLICATION_H
