#ifndef DECK_ASSISTANT_UTIL_SOCKET_H
#define DECK_ASSISTANT_UTIL_SOCKET_H

#include <memory>
#include <string_view>
#include <thread>

class Socket
{
public:
	enum class State : char
	{
		Disconnected,
		Connecting,
		Connected,
	};

	struct SharedState;

public:
	Socket();
	~Socket();

	bool start_connect(std::string_view const& host, int port);
	int read_nonblock(void* data, int maxlen);
	bool write(void const* data, int len);
	void close();

	State get_state() const;
	std::string_view get_last_error() const;

private:
	void close_impl();
	static void worker(std::stop_token stop_token, std::shared_ptr<SharedState> shared_state);

	std::jthread m_worker_thread;
	std::shared_ptr<SharedState> m_shared_state;
};

#endif // DECK_ASSISTANT_UTIL_SOCKET_H
