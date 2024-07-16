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

#include "util_tls_session.h"
#include "util_blob.h"
#include <string>

#if (defined HAVE_GNUTLS)
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#elif (defined HAVE_OPENSSL)
#include <openssl/ssl.h>
#endif

using namespace util;

struct util::TLSSession::State
{
	IO* io = nullptr;
	std::string remote_name;
	bool handshaking;

#if (defined HAVE_GNUTLS)
	gnutls_session_t session               = nullptr;
	gnutls_certificate_credentials_t creds = nullptr;
	BlobBuffer outbuffer;
	BlobBuffer inbuffer;
#elif (defined HAVE_OPENSSL)
	SSL* connection = nullptr;
	BIO* rbio       = nullptr;
	BIO* wbio       = nullptr;
#endif

	~State()
	{
#if (defined HAVE_GNUTLS)
		gnutls_certificate_free_credentials(creds);
		gnutls_deinit(session);
#elif (defined HAVE_OPENSSL)
		SSL_free(connection);
#endif
	}
};

namespace
{
#if (defined HAVE_GNUTLS)

int verify_func(gnutls_session_t session)
{
	return 0;
}

ssize_t push_func(gnutls_transport_ptr_t transport, void const* data, size_t size)
{
	TLSSession::State* state = reinterpret_cast<TLSSession::State*>(transport);
	state->outbuffer.write(data, size);
	return size;
}

ssize_t pull_func(gnutls_transport_ptr_t transport, void* data, size_t size)
{
	TLSSession::State* state = reinterpret_cast<TLSSession::State*>(transport);
	std::size_t count        = state->inbuffer.read(data, size);
	if (count == 0)
	{
		errno = EAGAIN;
		return -1;
	}
	else
	{
		return count;
	}
}

int state_init_as_client(TLSSession::State& state, bool verify_certificate)
{
	int last_error = gnutls_init(&state.session, GNUTLS_CLIENT | GNUTLS_NONBLOCK | GNUTLS_NO_SIGNAL | GNUTLS_POST_HANDSHAKE_AUTH | GNUTLS_AUTO_REAUTH);
	if (last_error != GNUTLS_E_SUCCESS)
		return last_error;

	last_error = gnutls_certificate_allocate_credentials(&state.creds);
	if (last_error != GNUTLS_E_SUCCESS)
		return last_error;

	gnutls_transport_set_ptr(state.session, &state);
	gnutls_transport_set_push_function(state.session, &push_func);
	gnutls_transport_set_pull_function(state.session, &pull_func);

	gnutls_ocsp_status_request_enable_client(state.session, nullptr, 0, nullptr);

	last_error = gnutls_set_default_priority(state.session);
	if (last_error != GNUTLS_E_SUCCESS)
		return last_error;

	if (!state.remote_name.empty())
	{
		gnutls_session_set_verify_cert(state.session, state.remote_name.c_str(), 0);

		last_error = gnutls_server_name_set(state.session, GNUTLS_NAME_DNS, state.remote_name.data(), state.remote_name.size());
		if (last_error != GNUTLS_E_SUCCESS)
			return last_error;
	}

	if (verify_certificate)
	{
		last_error = gnutls_certificate_set_x509_system_trust(state.creds);
		if (last_error < 0)
			return last_error;
	}
	else
	{
		gnutls_session_set_verify_function(state.session, &verify_func);
	}

	last_error = gnutls_credentials_set(state.session, GNUTLS_CRD_CERTIFICATE, state.creds);
	if (last_error != GNUTLS_E_SUCCESS)
		return last_error;

	gnutls_handshake_set_timeout(state.session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);

	// Max TLS frame is 16k
	state.inbuffer.reserve(17 * 1024);
	state.outbuffer.reserve(17 * 1024);

	last_error = gnutls_handshake(state.session);
	if (last_error == GNUTLS_E_INTERRUPTED || last_error == GNUTLS_E_AGAIN)
		last_error = GNUTLS_E_SUCCESS;

	return last_error;
}

#endif
} // namespace

TLSSession::TLSSession() = default;

TLSSession::TLSSession(TLSSession&&) = default;

TLSSession::~TLSSession() = default;

TLSSession& TLSSession::operator=(TLSSession&&) = default;

TLSSession::operator bool() const
{
	return m_state.get();
}

bool TLSSession::operator!() const
{
	return !m_state;
}

bool TLSSession::init_as_client(IO& io, std::string_view const& remote_name, bool verify_certificate)
{
	m_state.reset(new State);
	m_state->io          = &io;
	m_state->remote_name = remote_name;
	m_state->handshaking = true;

#if (defined HAVE_GNUTLS)
	m_last_error = state_init_as_client(*m_state, verify_certificate);
	if (m_last_error == GNUTLS_E_SUCCESS)
		return true;
#endif

	m_state.reset();
	return false;
}

void TLSSession::deinit()
{
	m_state.reset();
}

bool TLSSession::pump_read()
{
	if (!m_state)
		return false;

#if (defined HAVE_GNUTLS)
	BlobBuffer& inbuf = m_state->inbuffer;

	if (inbuf.space() < 4096)
		inbuf.flush();

	if (inbuf.space() < 4096)
		inbuf.reserve(inbuf.capacity() + 4096);

	int received = m_state->io->read(inbuf.tail(), inbuf.space());
	if (received < 0)
	{
		deinit();
		return false;
	}

	if (received > 0)
	{
		inbuf.added_to_tail(received);

		if (m_state->handshaking)
		{
			m_last_error = gnutls_handshake(m_state->session);
			if (gnutls_error_is_fatal(m_last_error))
			{
				deinit();
				return false;
			}

			if (m_last_error != GNUTLS_E_SUCCESS)
				return false;

			m_state->handshaking = false;
		}
	}

	return !inbuf.empty() || gnutls_record_check_pending(m_state->session) > 0;
#else
	return false;
#endif
}

bool TLSSession::pump_write()
{
	if (!m_state)
		return false;

#if (defined HAVE_GNUTLS)
	BlobBuffer& outbuf = m_state->outbuffer;
	if (!outbuf.empty())
	{
		int result = m_state->io->write(outbuf.data(), outbuf.size());
		if (result < 0)
		{
			deinit();
			return false;
		}

		if (result > 0)
			outbuf.advance(result);
	}
	return true;
#else
	return false;
#endif
}

bool TLSSession::is_handshaking() const
{
	return m_state && m_state->handshaking;
}

bool TLSSession::is_connected() const
{
	return m_state && !m_state->handshaking;
}

int TLSSession::read(void* data, int maxlen)
{
	if (!m_state)
		return -1;

	if (m_state->handshaking)
		return 0;

#if (defined HAVE_GNUTLS)
	ssize_t received = gnutls_record_recv(m_state->session, data, maxlen);
	if (received < 0)
	{
		if (received == GNUTLS_E_INTERRUPTED || received == GNUTLS_E_AGAIN)
			received = GNUTLS_E_SUCCESS;

		m_last_error = received;

		if (gnutls_error_is_fatal(m_last_error))
		{
			deinit();
			return -1;
		}

		received = 0;
	}

	return received;
#else
	return -1;
#endif
}

int TLSSession::write(void const* data, int len)
{
	if (!m_state)
		return -1;

	if (m_state->handshaking)
		return 0;

#if (defined HAVE_GNUTLS)
	ssize_t sent = gnutls_record_send(m_state->session, data, len);
	if (sent < 0)
	{
		m_last_error = sent;

		if (gnutls_error_is_fatal(m_last_error))
		{
			deinit();
			return -1;
		}

		sent = 0;
	}

	return sent;
#else
	return -1;
#endif
}

void TLSSession::shutdown()
{
	if (!m_state)
		return;

#if (defined HAVE_GNUTLS)
	gnutls_bye(m_state->session, GNUTLS_SHUT_WR);
	m_state->handshaking = false;
#endif
}

std::string_view TLSSession::get_last_error() const
{
#if (defined HAVE_GNUTLS)
	return gnutls_strerror(m_last_error);
#else
	return "No TLS implementation available";
#endif
}
