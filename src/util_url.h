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

#ifndef DECK_ASSISTANT_UTIL_URL_H
#define DECK_ASSISTANT_UTIL_URL_H

#include <string>
#include <string_view>

class URL
{
public:
	URL();
	~URL();

	bool set_connection_string(std::string_view const& conn_string);
	bool set_schema(std::string_view const& value);
	bool set_host(std::string_view const& value);
	bool set_path(std::string_view const& value);
	bool set_port(int port);

	inline std::string_view get_connection_string() const { return m_connection_string; }
	inline std::string_view get_schema() const { return m_schema; }
	inline std::string_view get_host() const { return m_host; }
	inline std::string_view get_path() const { return m_path; }
	inline int get_port() const { return m_port; }

private:
	void set_connection_string_normalized(std::string&& conn_string);

	std::string m_connection_string;
	std::string_view m_schema;
	std::string_view m_host;
	std::string_view m_path;
	int m_port;
};

#endif // DECK_ASSISTANT_UTIL_URL_H
