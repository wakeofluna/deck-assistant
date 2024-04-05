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
