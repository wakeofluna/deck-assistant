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

#include "util_socket.h"
#include <SDL_net.h>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <mutex>
#include <string>

using namespace util;

struct Socket::SharedState
{
	SharedState()
	    : state(State::Disconnected)
	    , port(0)
	    , socket(nullptr)
	    , socket_set(nullptr)
	{
	}

	State state;
	std::string_view last_error;
	std::string last_error_buffer;
	std::mutex mutex;

	std::string host;
	int port;

	TCPsocket socket;
	SDLNet_SocketSet socket_set;
};

Socket::Socket()
    : m_shared_state(new SharedState)
{
}

Socket::~Socket()
{
	close();
}

bool Socket::start_connect(std::string_view const& host, int port)
{
	std::lock_guard guard(m_shared_state->mutex);

	if (m_shared_state->state != State::Disconnected)
	{
		m_shared_state->last_error = "Socket is busy";
		return false;
	}

	if (m_worker_thread.joinable())
		m_worker_thread.join();

	m_shared_state->host = host;
	m_shared_state->port = port;
	m_shared_state->last_error_buffer.clear();
	m_shared_state->last_error = std::string_view();
	m_shared_state->state      = State::Connecting;
	m_worker_thread            = std::jthread(&worker, m_shared_state);
	return true;
}

int Socket::read_nonblock(void* data, int maxlen)
{
	std::lock_guard guard(m_shared_state->mutex);

	if (!m_shared_state->socket)
	{
		m_shared_state->last_error = "Socket is not connected";
		close_impl();
		return -1;
	}

	int ready = SDLNet_CheckSockets(m_shared_state->socket_set, 0);
	if (!ready)
		return 0;

	int result = SDLNet_TCP_Recv(m_shared_state->socket, data, maxlen);
	if (result == 0)
	{
		m_shared_state->last_error = "Socket EOF";
		close_impl();
		return -1;
	}
	else if (result < 0)
	{
		m_shared_state->last_error_buffer = SDLNet_GetError();
		m_shared_state->last_error        = m_shared_state->last_error_buffer;
		close_impl();
		return -1;
	}

	return result;
}

bool Socket::write(void const* data, int len)
{
	std::lock_guard guard(m_shared_state->mutex);

	if (!m_shared_state->socket)
	{
		m_shared_state->last_error = "Socket is not connected";
		close_impl();
		return false;
	}

	int result = SDLNet_TCP_Send(m_shared_state->socket, data, len);
	if (result < len)
	{
		m_shared_state->last_error_buffer = SDLNet_GetError();
		m_shared_state->last_error        = m_shared_state->last_error_buffer;
		close_impl();
		return false;
	}

	return true;
}

void Socket::close()
{
	std::lock_guard guard(m_shared_state->mutex);
	close_impl();
}

Socket::State Socket::get_state() const
{
	return m_shared_state->state;
}

std::string_view Socket::get_last_error() const
{
	return m_shared_state->last_error;
}

void Socket::close_impl()
{
	if (m_worker_thread.joinable())
	{
		m_worker_thread.request_stop();
		m_worker_thread.detach();
	}

	if (m_shared_state->socket_set)
	{
		SDLNet_FreeSocketSet(m_shared_state->socket_set);
		m_shared_state->socket_set = nullptr;
	}

	if (m_shared_state->socket)
	{
		SDLNet_TCP_Close(m_shared_state->socket);
		m_shared_state->socket = nullptr;
	}

	m_shared_state->state = State::Disconnected;
}

void Socket::worker(std::stop_token stop_token, std::shared_ptr<SharedState> shared_state)
{
	std::unique_lock guard(shared_state->mutex);

	if (stop_token.stop_requested())
		return;

	std::string host = shared_state->host;
	int port         = shared_state->port;

	TCPsocket socket;
	IPaddress address;
	int result;

	guard.unlock();
	result = SDLNet_ResolveHost(&address, host.empty() ? "localhost" : host.c_str(), port);
	guard.lock();

	if (stop_token.stop_requested())
		return;

	if (result == -1)
	{
		shared_state->last_error_buffer = SDLNet_GetError();
		shared_state->last_error        = shared_state->last_error_buffer;
		shared_state->state             = State::Disconnected;
		return;
	}

	guard.unlock();
	socket = SDLNet_TCP_Open(&address);
	guard.lock();

	if (stop_token.stop_requested())
	{
		if (socket)
			SDLNet_TCP_Close(socket);

		return;
	}

	if (!socket)
	{
		shared_state->last_error_buffer = SDLNet_GetError();
		shared_state->last_error        = shared_state->last_error_buffer;
		shared_state->state             = State::Disconnected;
		return;
	}

	shared_state->socket = socket;

	if (!shared_state->socket_set)
		shared_state->socket_set = SDLNet_AllocSocketSet(1);

	SDLNet_AddSocket(shared_state->socket_set, (SDLNet_GenericSocket)shared_state->socket);

	shared_state->last_error = std::string_view();
	shared_state->state      = State::Connected;
}
