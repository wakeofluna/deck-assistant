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

#ifndef DECK_ASSISTANT_UTIL_SOCKET_H
#define DECK_ASSISTANT_UTIL_SOCKET_H

#include <memory>
#include <optional>
#include <string_view>
#include <thread>

namespace util
{

class SocketSet;

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
	Socket(std::shared_ptr<SocketSet> const& socket_set);
	Socket(Socket const&) = delete;
	Socket(Socket&& other);
	~Socket();

	Socket& operator=(Socket const&) = delete;
	Socket& operator=(Socket&&)      = delete;

	bool start_connect(std::string_view const& host, int port);
	int read_nonblock(void* data, int maxlen);
	bool write(void const* data, int len);
	void close();

	std::optional<Socket> accept_nonblock();

	State get_state() const;
	std::string const& get_remote_host() const;
	int get_remote_port() const;
	std::string_view get_last_error() const;

private:
	void close_impl();
	static void worker(std::stop_token stop_token, std::shared_ptr<SharedState> shared_state);

	std::jthread m_worker_thread;
	std::shared_ptr<SharedState> m_shared_state;
};

class SocketSet
{
private:
	SocketSet(int max_sockets);

public:
	SocketSet(SocketSet const& other) = delete;
	SocketSet(SocketSet&& other)      = delete;
	~SocketSet();

	static std::shared_ptr<SocketSet> create(int max_sockets);
	bool poll(int timeout_msec = 0) const;

	SocketSet& operator=(SocketSet const& other) = delete;
	SocketSet& operator=(SocketSet&& other)      = delete;

private:
	friend class Socket;
	void* m_data;
};

} // namespace util

#endif // DECK_ASSISTANT_UTIL_SOCKET_H
