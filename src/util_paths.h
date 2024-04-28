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

#ifndef DECK_ASSISTANT_UTIL_PATHS_H
#define DECK_ASSISTANT_UTIL_PATHS_H

#include <filesystem>
#include <string_view>
#include <vector>

class Paths
{
public:
	Paths();
	~Paths();

	void resolve_standard_paths();
	void set_sandbox_path(std::filesystem::path path);

	std::filesystem::path find_data_file(std::string_view const& file_name, bool allow_home, bool allow_system) const;
	std::filesystem::path find_config_file(std::string_view const& file_name, bool allow_home) const;
	std::filesystem::path find_executable(std::string_view const& file_name, bool allow_path) const;

	inline std::filesystem::path const& get_sandbox_dir() const { return m_sandbox_dir; }
	inline std::filesystem::path const& get_user_data_dir() const { return m_user_data_dir; }
	inline std::filesystem::path const& get_user_config_dir() const { return m_user_config_dir; }
	inline std::vector<std::filesystem::path> const& get_system_path_dirs() const { return m_system_path_dirs; }
	inline std::vector<std::filesystem::path> const& get_system_data_dirs() const { return m_system_data_dirs; }

	static bool verify_path_contains_path(std::filesystem::path const& p, std::filesystem::path const& base, bool allow_subdirs = true);

private:
	std::filesystem::path m_sandbox_dir;
	std::filesystem::path m_user_data_dir;
	std::filesystem::path m_user_config_dir;
	std::vector<std::filesystem::path> m_system_path_dirs;
	std::vector<std::filesystem::path> m_system_data_dirs;
};

#endif // DECK_ASSISTANT_UTIL_PATHS_H
