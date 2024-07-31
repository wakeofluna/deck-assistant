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
#include "util_tls_session.h"
#include <SDL_net.h>
#include <cassert>
#include <fcntl.h>
#include <mutex>
#include <string>

using namespace util;

struct Socket::SharedState : public TLSSession::IO
{
	SharedState()
	    : state(State::Disconnected)
	    , port(0)
	    , use_tls(TLS::NoTLS)
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
	TLS use_tls;

	TCPsocket socket;
	std::shared_ptr<SocketSet> socket_set;
	TLSSession tls_session;

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

	int read(void* data, int maxlen) override
	{
		if (!socket)
		{
			last_error = "Socket is not connected";
			return -1;
		}

		if (!SDLNet_SocketReady(socket))
			return 0;

		int result = SDLNet_TCP_Recv(socket, data, maxlen);
		return set_result(result);
	}

	int write(void const* data, int len) override
	{
		if (len <= 0)
			return 0;

		if (!socket)
		{
			last_error = "Socket is not connected";
			return -1;
		}

		int result = SDLNet_TCP_Send(socket, data, len);
		return set_result(result);
	}

	void close()
	{
		if (socket)
		{
			SDLNet_TCP_DelSocket(reinterpret_cast<SDLNet_SocketSet>(socket_set->m_data), socket);
			SDLNet_TCP_Close(socket);
			socket = nullptr;
		}
		state = State::Disconnected;
		tls_session.deinit();
	}

private:
	int set_result(int result)
	{
		if (result == 0)
		{
			last_error = "Socket EOF";
			close();
			return -1;
		}
		else if (result < 0)
		{
			last_error_buffer = SDLNet_GetError();
			last_error        = last_error_buffer;
			close();
			return -1;
		}
		return result;
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

Socket::TLS Socket::get_tls() const
{
	return m_shared_state->use_tls;
}

bool Socket::set_tls(TLS use_tls)
{
	std::lock_guard guard(m_shared_state->mutex);
#if (defined HAVE_GNUTLS || defined HAVE_OPENSSL)
	m_shared_state->use_tls = use_tls;
	return true;
#else
	m_shared_state->last_error = "TLS connection not supported, no gnutls or openssl support";
	return false;
#endif
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

void Socket::tls_handshake()
{
	std::lock_guard guard(m_shared_state->mutex);

	m_shared_state->tls_session.pump_read();
	m_shared_state->tls_session.pump_write();

	if (!m_shared_state->tls_session)
	{
		m_shared_state->last_error_buffer = m_shared_state->tls_session.get_last_error();
		m_shared_state->last_error        = m_shared_state->last_error_buffer;
		close_impl();
	}
	else if (m_shared_state->tls_session.is_connected())
	{
		m_shared_state->state = State::Connected;
	}
}

int Socket::read_nonblock(void* data, int maxlen)
{
	std::lock_guard guard(m_shared_state->mutex);

	if (m_shared_state->use_tls != TLS::NoTLS)
	{
		bool pumped = m_shared_state->tls_session.pump_read();
		m_shared_state->tls_session.pump_write();

		if (pumped)
		{
			int received = m_shared_state->tls_session.read(data, maxlen);
			if (received > 0)
				return received;
		}

		if (!m_shared_state->tls_session)
		{
			m_shared_state->last_error_buffer = m_shared_state->tls_session.get_last_error();
			m_shared_state->last_error        = m_shared_state->last_error_buffer;
			close_impl();
			return -1;
		}

		return 0;
	}
	else
	{
		return m_shared_state->read(data, maxlen);
	}
}

bool Socket::write(void const* data, int len)
{
	if (len <= 0)
		return true;

	std::lock_guard guard(m_shared_state->mutex);

	int written;
	if (m_shared_state->use_tls != TLS::NoTLS)
	{
		written = m_shared_state->tls_session.write(data, len);
		if (written > 0)
			m_shared_state->tls_session.pump_write();

		if (!m_shared_state->tls_session)
		{
			m_shared_state->last_error_buffer = m_shared_state->tls_session.get_last_error();
			m_shared_state->last_error        = m_shared_state->last_error_buffer;
			close_impl();
		}
	}
	else
	{
		written = m_shared_state->write(data, len);
	}

	assert(written < 0 || written == len);
	return (written == len);
}

void Socket::shutdown()
{
	if (m_shared_state)
	{
		std::lock_guard guard(m_shared_state->mutex);
		m_shared_state->tls_session.shutdown();
		m_shared_state->tls_session.pump_write();
	}
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
	client_state->fill_remote_address();
	client_state->state = State::Connected;

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
	m_shared_state->close();
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

	bool const is_server = host.empty();
	if (!is_server)
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

	if (shared_state->use_tls != TLS::NoTLS && !is_server)
	{
		bool tls_ok = shared_state->tls_session.init_as_client(*shared_state, host, shared_state->use_tls == TLS::TLS);
		if (!tls_ok)
		{
			shared_state->last_error_buffer = shared_state->tls_session.get_last_error();
			shared_state->last_error        = shared_state->last_error_buffer;
			shared_state->state             = State::Disconnected;
			return;
		}
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
	shared_state->state      = shared_state->use_tls != TLS::NoTLS ? State::TLSHandshaking : State::Connected;
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
