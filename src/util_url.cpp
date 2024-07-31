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

#include "util_url.h"
#include <cassert>
#include <charconv>

using namespace util;

namespace
{

std::string build_connection_string(std::string_view const& schema, std::string_view const& host, int port, std::string_view const& path)
{
	std::string result;
	result.reserve(128);

	if (schema.empty())
	{
		result = "https://";
	}
	else
	{
		result = schema;
		if (!schema.ends_with("://"))
			result += "://";
	}

	if (!host.empty() || port != 0)
	{
		if (host.empty())
			result += "localhost";
		else
			result += host;

		if (port > 0)
			result += ':' + std::to_string(port);
	}

	if (path.empty())
	{
		result += '/';
	}
	else
	{
		if (path[0] != '/')
			result += '/';
		result += path;
	}

	return result;
}

bool parse_connection_string(std::string_view const& conn_string, std::string_view& schema, std::string_view& host, int& port, std::string_view& path)
{
	std::size_t cursor = 0;
	std::size_t found;
	std::size_t found2;
	bool is_normalized = true;

	found = conn_string.find(':', cursor);
	if (found == std::string_view::npos || conn_string.substr(found, 3) != "://")
	{
		schema        = std::string_view();
		is_normalized = false;
	}
	else
	{
		schema = conn_string.substr(cursor, found - cursor);
		cursor = found + 3;
	}

	found = conn_string.find('/', cursor);
	if (found == std::string_view::npos)
	{
		is_normalized = false;
		path          = "/";
		found         = conn_string.size();
	}
	else
	{
		path = conn_string.substr(found);
	}

	found2 = conn_string.rfind(':', found);
	if (found2 < cursor)
		found2 = std::string_view::npos;

	port = 0;
	if (found2 == std::string_view::npos)
	{
		host = conn_string.substr(cursor, found - cursor);
	}
	else
	{
		host = conn_string.substr(cursor, found2 - cursor);

		std::string_view port_str = conn_string.substr(found2 + 1, found - found2 - 1);
		auto [ptr, ec]            = std::from_chars(port_str.data(), port_str.data() + port_str.size(), port, 10);

		if (ec != std::errc() || ptr != port_str.data() + port_str.size())
			is_normalized = false;
	}

	return is_normalized;
}

} // namespace

URL::URL()
    : m_port(0)
{
}

URL::URL(URL&& other) = default;

URL::~URL() = default;

URL& URL::operator=(URL&& other) = default;

bool URL::set_connection_string(std::string_view const& conn_string, std::string_view const& default_schema)
{
	std::string_view schema;
	std::string_view host;
	int port = 0;
	std::string_view path;

	parse_connection_string(conn_string, schema, host, port, path);

	if (schema.empty())
		schema = default_schema;

	std::string new_conn_string = build_connection_string(schema, host, port, path);
	set_connection_string_normalized(std::move(new_conn_string));
	return true;
}

bool URL::set_schema(std::string_view const& value)
{
	if (value.find_first_of(":/") != std::string_view::npos)
		return false;

	std::string new_conn_string = build_connection_string(value, m_host, m_port, m_path);
	set_connection_string_normalized(std::move(new_conn_string));
	return true;
}

bool URL::set_host(std::string_view const& value)
{
	if (value.find_first_of(":/") != std::string_view::npos)
		return false;

	std::string new_conn_string = build_connection_string(m_schema, value, m_port, m_path);
	set_connection_string_normalized(std::move(new_conn_string));
	return true;
}

bool URL::set_path(std::string_view const& value)
{
	std::string new_conn_string = build_connection_string(m_schema, m_host, m_port, value);
	set_connection_string_normalized(std::move(new_conn_string));
	return true;
}

bool URL::set_port(int port)
{
	if (port < 0 || port > 65535)
		return false;

	std::string new_conn_string = build_connection_string(m_schema, m_host, port, m_path);
	set_connection_string_normalized(std::move(new_conn_string));
	return true;
}

void URL::set_connection_string_normalized(std::string&& conn_string)
{
	std::string_view schema;
	std::string_view host;
	int port;
	std::string_view path;

	bool is_normalized = parse_connection_string(conn_string, schema, host, port, path);
	assert(is_normalized && "connection string is not normalized");

	if (is_normalized)
	{
		m_connection_string.swap(conn_string);
		m_schema = schema;
		m_host   = host;
		m_port   = port;
		m_path   = path;
	}
}
