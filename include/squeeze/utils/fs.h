// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <string_view>
#include <variant>
#include <fstream>
#include <filesystem>

#include "squeeze/status.h"
#include "squeeze/entry_common.h"

namespace squeeze::utils {

std::variant<std::fstream, StatCode> make_regular_file(std::string_view path);
std::variant<std::ofstream, StatCode> make_regular_file_out(std::string_view path);
StatCode make_directory(std::string_view path, EntryPermissions perms);
StatCode make_symlink(std::string_view path, std::string_view link_to, EntryPermissions perms);
StatCode set_permissions(const std::filesystem::path& path, EntryPermissions perms);

void convert(const EntryPermissions& from, std::filesystem::perms& to);
void convert(const std::filesystem::perms& from, EntryPermissions& to);

void convert(const EntryType& from, std::filesystem::file_type& to);
void convert(const std::filesystem::file_type& from, EntryType& to);

[[nodiscard]]
std::optional<std::string> make_concise_portable_path(const std::filesystem::path& path);

[[nodiscard]]
bool path_within_dir(const std::string_view path, const std::string_view dir);

}
