#pragma once

#include <string_view>
#include <variant>
#include <fstream>
#include <filesystem>

#include "squeeze/error.h"
#include "squeeze/entry.h"

namespace squeeze::utils {

std::variant<std::fstream, ErrorCode>
    make_regular_file(std::string_view path, EntryPermissions perms);
ErrorCode make_directory(std::string_view path, EntryPermissions perms);
ErrorCode make_symlink(std::string_view path, std::string_view link_to, EntryPermissions perms);

void convert(const EntryPermissions& from, std::filesystem::perms& to);
void convert(const std::filesystem::perms& from, EntryPermissions& to);

}
