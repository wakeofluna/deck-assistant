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
