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

#ifndef DECK_ASSISTANT_UTIL_TLS_SESSION_H
#define DECK_ASSISTANT_UTIL_TLS_SESSION_H

#include <memory>
#include <string_view>

namespace util
{

class TLSSession
{
public:
	struct IO
	{
		virtual int read(void* data, int maxlen)     = 0;
		virtual int write(void const* data, int len) = 0;
	};

public:
	TLSSession();
	TLSSession(TLSSession const&) = delete;
	TLSSession(TLSSession&&);
	~TLSSession();

	TLSSession& operator=(TLSSession const&) = delete;
	TLSSession& operator=(TLSSession&&);
	operator bool() const;
	bool operator!() const;

	bool init_as_client(IO& io, std::string_view const& remote_name, bool verify_certificate = true);
	// bool init_as_server(IO& io, std::string_view const& certificate);
	void deinit();

	bool pump_read();
	bool pump_write();
	bool is_handshaking() const;
	bool is_connected() const;

	int read(void* data, int maxlen);
	int write(void const* data, int len);
	void shutdown();

	std::string_view get_last_error() const;

public:
	struct State;
	std::unique_ptr<State> m_state;
	int m_last_error;
};

} // namespace util

#endif // DECK_ASSISTANT_UTIL_TLS_SESSION_H
