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
#include <cassert>
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
	{
	}

	~SharedState()
	{
		assert(!socket && "socket not closed at SharedState destruction");
	}

	State state;
	std::string_view last_error;
	std::string last_error_buffer;
	std::mutex mutex;

	std::string host;
	int port;

	TCPsocket socket;
	std::shared_ptr<SocketSet> socket_set;

	void fill_remote_address()
	{
		assert(socket);

		host.clear();
		port = 0;

		IPaddress const* address = SDLNet_TCP_GetPeerAddress(socket);
		if (address)
		{
			union
			{
				struct
				{
					Uint8 a, b, c, d;
				} parts;
				Uint32 ip;
			} helper;

			helper.ip = address->host;

			host.reserve(16);
			host  = std::to_string(helper.parts.a);
			host += '.';
			host += std::to_string(helper.parts.b);
			host += '.';
			host += std::to_string(helper.parts.c);
			host += '.';
			host += std::to_string(helper.parts.d);

			port = SDL_SwapBE16(address->port);
		}
	}
};

Socket::Socket(std::shared_ptr<SocketSet> const& socket_set)
    : m_shared_state(new SharedState)
{
	assert(m_shared_state && "failed to allocate shared state for new Socket instance");
	m_shared_state->socket_set = socket_set;
}

Socket::Socket(Socket&& other)
{
	m_shared_state.swap(other.m_shared_state);
	m_worker_thread.swap(other.m_worker_thread);
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

	if (!SDLNet_SocketReady(m_shared_state->socket))
		return -1;

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
	if (m_shared_state)
	{
		std::lock_guard guard(m_shared_state->mutex);
		close_impl();
	}
}

std::optional<Socket> Socket::accept_nonblock()
{
	std::lock_guard guard(m_shared_state->mutex);

	if (!m_shared_state->socket)
	{
		m_shared_state->last_error = "Socket is not connected";
		close_impl();
		return std::nullopt;
	}

	if (!SDLNet_SocketReady(m_shared_state->socket))
		return std::nullopt;

	TCPsocket new_socket = SDLNet_TCP_Accept(m_shared_state->socket);
	if (!new_socket)
	{
		m_shared_state->last_error_buffer = SDLNet_GetError();
		m_shared_state->last_error        = m_shared_state->last_error_buffer;
		close_impl();
		return std::nullopt;
	}

	if (SDLNet_TCP_AddSocket(reinterpret_cast<SDLNet_SocketSet>(m_shared_state->socket_set->m_data), new_socket) == -1)
	{
		SDLNet_TCP_Close(new_socket);
		m_shared_state->last_error_buffer.clear();
		m_shared_state->last_error = "unable to accept new client: SocketSet is full";
		return std::nullopt;
	}

	std::optional<Socket> client = std::make_optional<Socket>(m_shared_state->socket_set);

	SharedState* client_state = client->m_shared_state.get();
	client_state->socket      = new_socket;
	client_state->state       = State::Connected;
	client_state->fill_remote_address();

	return client;
}

Socket::State Socket::get_state() const
{
	return m_shared_state->state;
}

std::string const& Socket::get_remote_host() const
{
	return m_shared_state->host;
}

int Socket::get_remote_port() const
{
	return m_shared_state->port;
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

	if (m_shared_state->socket)
	{
		SDLNet_TCP_DelSocket(reinterpret_cast<SDLNet_SocketSet>(m_shared_state->socket_set->m_data), m_shared_state->socket);
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

	if (!host.empty())
	{
		guard.unlock();
		result = SDLNet_ResolveHost(&address, host.c_str(), port);
		guard.lock();

		if (stop_token.stop_requested())
			return;
	}
	else
	{
		address.host = INADDR_ANY;
		address.port = SDL_SwapBE16(port);
		result       = 0;
	}

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

	if (SDLNet_TCP_AddSocket(reinterpret_cast<SDLNet_SocketSet>(shared_state->socket_set->m_data), socket) == -1)
	{
		SDLNet_TCP_Close(socket);
		shared_state->last_error = "unable to connect socket: SocketSet is full";
		return;
	}

	shared_state->socket     = socket;
	shared_state->last_error = std::string_view();
	shared_state->state      = State::Connected;
}

SocketSet::SocketSet(int max_sockets)
{
	assert(max_sockets > 0);
	m_data = SDLNet_AllocSocketSet(max_sockets);
}

SocketSet::~SocketSet()
{
	SDLNet_FreeSocketSet(reinterpret_cast<SDLNet_SocketSet>(m_data));
}

std::shared_ptr<SocketSet> SocketSet::create(int max_sockets)
{
	struct enabler : public SocketSet
	{
		enabler(int max_sockets)
		    : SocketSet(max_sockets)
		{
		}
	};

	return std::make_shared<enabler>(max_sockets);
}

bool SocketSet::poll(int timeout_msec) const
{
	return SDLNet_CheckSockets(reinterpret_cast<SDLNet_SocketSet>(m_data), timeout_msec) > 0;
}
