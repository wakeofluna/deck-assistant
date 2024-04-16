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

#include "util_paths.h"
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <processenv.h>
#include <windows.h>
#else
#include <wordexp.h>
#endif

namespace
{

#ifdef _WIN32
using fs_char_type = wchar_t;
#else
using fs_char_type = char;
#endif

std::vector<std::filesystem::path> resolve_paths(fs_char_type const* env_name, fs_char_type const* fallback)
{
	using fs_string_view = std::basic_string_view<fs_char_type>;
	using fs_string      = std::basic_string<fs_char_type>;
	using fs_size_type   = typename fs_string_view::size_type;

	std::vector<std::filesystem::path> results;
	fs_string_view paths;

#ifdef _WIN32
	fs_char_type const paths_split_char = ';';
	fs_string env_paths_buf;
	env_paths_buf.assign(2048, 0);
	DWORD len = GetEnvironmentVariableW(env_name, env_paths_buf.data(), env_paths_buf.size());
	if (len > 0 && len < env_paths_buf.size())
		paths = fs_string_view(env_paths_buf.data(), len);
#else
	fs_char_type const paths_split_char = ':';
	wordexp_t wordexp_data {};
	if (fs_char_type const* env_paths = std::getenv(env_name); env_paths)
		paths = env_paths;
#endif
	if (paths.empty() && fallback)
		paths = fallback;

	fs_size_type offset = 0;
	while (offset < paths.size())
	{
		fs_size_type next = paths.find(paths_split_char, offset);
		if (next == fs_string_view::npos)
			next = paths.size();

		fs_string_view cur_path = paths.substr(offset, next - offset);
		fs_string_view expanded_path;

#ifdef _WIN32
		fs_string need_expand_path(cur_path);
		fs_string expand_buf;
		expand_buf.assign(2048, 0);
		DWORD expand_len = ExpandEnvironmentStringsW(need_expand_path.c_str(), expand_buf.data(), expand_buf.size());
		if (expand_len > 0)
			expanded_path = fs_string_view(expand_buf.data(), expand_len);
#else
		fs_string need_expand_path(cur_path);
		int wordexp_result = wordexp(need_expand_path.c_str(), &wordexp_data, WRDE_NOCMD | (offset > 0 ? WRDE_REUSE : 0));
		if (wordexp_result == 0 && wordexp_data.we_wordc > 0)
			expanded_path = wordexp_data.we_wordv[0];
#endif

		if (!expanded_path.empty())
		{
			std::error_code ec;
			std::filesystem::path abs_path = std::filesystem::absolute(expanded_path, ec);
			if (!ec && !abs_path.empty())
				results.emplace_back(std::move(abs_path));
		}

		offset = next + 1;
	}

#ifndef _WIN32
	wordfree(&wordexp_data);
#endif

	return results;
}

void append_and_or_filter_paths(std::vector<std::filesystem::path>& vec, char const* append_path, bool filter_existing)
{
	if (append_path)
	{
		for (std::filesystem::path& path : vec)
			path /= append_path;
	}

	if (filter_existing)
	{
		std::vector<std::filesystem::path> old(std::move(vec));
		vec.clear();
		vec.reserve(old.size());

		std::error_code ec;
		for (std::filesystem::path& path : old)
		{
			if (std::filesystem::exists(path, ec))
				vec.emplace_back(std::move(path));
		}
	}
}

bool find_file_in(std::filesystem::path const& base, std::string_view const& file_name, std::filesystem::path& target, bool allow_subdirs = true)
{
	std::error_code ec;

	target = std::filesystem::absolute(base / file_name, ec);
	if (ec)
		return false;

	// All base components must match and be used up
	auto base_iter   = base.begin();
	auto base_end    = base.end();
	auto target_iter = target.begin();
	auto target_end  = target.end();

	for (; base_iter != base_end && target_iter != target_end; ++base_iter, ++target_iter)
		if (*base_iter != *target_iter)
			return false;

	if (base_iter != base_end)
		return false;

	// Only one component allowed on the target if we don't allow subdirs
	if (!allow_subdirs)
	{
		++target_iter;
		if (target_iter != target_end)
			return false;
	}

	// Only actually access the filesystem once our checks have completed
	std::filesystem::file_status file_status = std::filesystem::status(target, ec);
	if (ec)
		return false;

	// Only allow files
	if (file_status.type() != std::filesystem::file_type::regular)
		return false;

	target.make_preferred();
	return true;
}

} // namespace

Paths::Paths()
{
	resolve_standard_paths();
}

Paths::~Paths() = default;

void Paths::resolve_standard_paths()
{
	std::vector<std::filesystem::path> tmp;

#ifdef _WIN32

	tmp = resolve_paths(L"XDG_CONFIG_HOME", L"%APPDATA%");
	if (!tmp.empty())
		m_user_config_dir = tmp[0];

	tmp = resolve_paths(L"XDG_DATA_HOME", L"%APPDATA%");
	if (!tmp.empty())
		m_user_data_dir = tmp[0];

	m_system_path_dirs = resolve_paths(L"PATH", nullptr);

	m_system_data_dirs = resolve_paths(L"XDG_DATA_DIRS", L"%ALLUSERSPROFILE%");

#else

	tmp = resolve_paths("XDG_CONFIG_HOME", "$HOME/.config");
	if (!tmp.empty())
		m_user_config_dir = tmp[0];

	tmp = resolve_paths("XDG_DATA_HOME", "$HOME/.local/share");
	if (!tmp.empty())
		m_user_data_dir = tmp[0];

	m_system_path_dirs = resolve_paths("PATH", nullptr);

	m_system_data_dirs = resolve_paths("XDG_DATA_DIRS", "/usr/local/share:/usr/share");

#endif

	if (!m_user_config_dir.empty())
		m_user_config_dir /= "deck-assistant";

	if (!m_user_data_dir.empty())
		m_user_data_dir /= "deck-assistant";

	append_and_or_filter_paths(m_system_path_dirs, nullptr, true);

	append_and_or_filter_paths(m_system_data_dirs, "deck-assistant", true);
}

void Paths::set_sandbox_path(std::filesystem::path path)
{
	m_sandbox_dir = std::move(path);
}

std::filesystem::path Paths::find_data_file(std::string_view const& file_name, bool allow_home, bool allow_system) const
{
	std::filesystem::path target;

	if (find_file_in(m_sandbox_dir, file_name, target))
		return target;

	if (allow_home && find_file_in(m_user_data_dir, file_name, target))
		return target;

	if (allow_system)
		for (std::filesystem::path const& base : m_system_data_dirs)
			if (find_file_in(base, file_name, target))
				return target;

	return std::filesystem::path();
}

std::filesystem::path Paths::find_config_file(std::string_view const& file_name, bool allow_home) const
{
	std::filesystem::path target;

	if (find_file_in(m_sandbox_dir, file_name, target))
		return target;

	if (allow_home && find_file_in(m_user_config_dir, file_name, target))
		return target;

	return std::filesystem::path();
}

std::filesystem::path Paths::find_executable(std::string_view const& file_name, bool allow_path) const
{
	std::filesystem::path target;

	if (find_file_in(m_sandbox_dir, file_name, target))
		return target;

	if (allow_path)
		for (std::filesystem::path const& base : m_system_path_dirs)
			if (find_file_in(base, file_name, target, false))
				return target;

	return std::filesystem::path();
}
