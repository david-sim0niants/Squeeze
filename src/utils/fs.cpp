#include "squeeze/utils/fs.h"

#include <filesystem>
#include <system_error>

#include "squeeze/utils/enum.h"

namespace squeeze::utils {

namespace fs = std::filesystem;

static std::error_code create_directories(const fs::path& path);

std::variant<std::fstream, StatCode> make_regular_file(std::string_view path_str)
{
    fs::path path(path_str);
    std::error_code ec = utils::create_directories(path.parent_path());
    if (ec)
        return ec;

    std::fstream file(path, std::ios_base::binary | std::ios_base::in | std::ios_base::out);
    if (!file)
        return std::make_error_code(std::errc::io_error);
    return file;
}

std::variant<std::ofstream, StatCode> make_regular_file_out(std::string_view path_str)
{
    fs::path path(path_str);
    std::error_code ec = utils::create_directories(path.parent_path());
    if (ec)
        return ec;

    std::filesystem::remove(path, ec);
    if (ec) {
        std::filesystem::permissions(path, std::filesystem::perms::owner_write);
        std::filesystem::remove(path, ec);
        if (ec)
            return ec;
    }

    std::ofstream file(path, std::ios_base::binary);
    if (!file)
        return std::make_error_code(std::errc::io_error);

    file.seekp(0, std::ios::beg);
    return file;
}

StatCode make_directory(std::string_view path_str, EntryPermissions entry_perms)
{
    fs::path path(path_str);
    std::error_code ec = utils::create_directories(path);
    if (ec)
        return ec;
    return set_permissions(path, entry_perms);
}

StatCode make_symlink(std::string_view path_str, std::string_view link_to,
        EntryPermissions entry_perms)
{
    fs::path path(path_str);
    std::error_code ec = utils::create_directories(path.parent_path());
    if (ec)
        return ec;

    if (fs::is_symlink(path, ec)) {
        fs::remove(path, ec);
        if (ec)
            return ec;
    }
    fs::create_symlink(fs::path(link_to), path, ec);
    if (ec)
        return ec;

    return success;
}

static std::error_code create_directories(const fs::path& path)
{
    std::error_code ec;
    if (path.empty())
        return ec;
    fs::create_directories(path, ec);
    return ec;
}

StatCode set_permissions(const fs::path &path, EntryPermissions entry_perms)
{
    std::error_code ec;
    fs::perms perms;
    convert(entry_perms, perms);
    fs::permissions(path, perms, ec);
    if (ec)
        return ec;
    return success;
}

void convert(const EntryPermissions& from, fs::perms& to)
{
    using enum EntryPermissions;
    using enum fs::perms;
    to = none
        | switch_flag(owner_read,   test_flag(from, OwnerRead))
        | switch_flag(owner_write,  test_flag(from, OwnerWrite))
        | switch_flag(owner_exec,   test_flag(from, OwnerExec))
        | switch_flag(group_read,   test_flag(from, GroupRead))
        | switch_flag(group_write,  test_flag(from, GroupWrite))
        | switch_flag(group_exec,   test_flag(from, GroupExec))
        | switch_flag(others_read,  test_flag(from, OthersRead))
        | switch_flag(others_write, test_flag(from, OthersWrite))
        | switch_flag(others_exec,  test_flag(from, OthersExec))
        ;
}

void convert(const fs::perms& from, EntryPermissions& to)
{
    using enum fs::perms;
    using enum EntryPermissions;
    to = None
        | switch_flag(OwnerRead,   test_flag(from, owner_read))
        | switch_flag(OwnerWrite,  test_flag(from, owner_write))
        | switch_flag(OwnerExec,   test_flag(from, owner_exec))
        | switch_flag(GroupRead,   test_flag(from, group_read))
        | switch_flag(GroupWrite,  test_flag(from, group_write))
        | switch_flag(GroupExec,   test_flag(from, group_exec))
        | switch_flag(OthersRead,  test_flag(from, others_read))
        | switch_flag(OthersWrite, test_flag(from, others_write))
        | switch_flag(OthersExec,  test_flag(from, others_exec))
        ;
}

void convert(const EntryType& from, fs::file_type& to)
{
    using enum EntryType;
    using enum fs::file_type;
    switch (from) {
    case None:
        to = none;
        break;
    case RegularFile:
        to = regular;
        break;
    case Directory:
        to = directory;
        break;
    case Symlink:
        to = symlink;
        break;
    default:
        to = unknown;
        break;
    }
}

void convert(const fs::file_type& from, EntryType& to)
{
    using enum EntryType;
    using enum fs::file_type;
    switch (from) {
    case none:
        to = None;
        break;
    case regular:
        to = RegularFile;
        break;
    case directory:
        to = Directory;
        break;
    case symlink:
        to = Symlink;
        break;
    default:
        to = None;
        break;
    }
}

std::optional<std::string> make_concise_portable_path(const fs::path& original_path)
{
    fs::path path = original_path.lexically_normal();
    std::error_code ec;
    auto status = fs::symlink_status(path, ec);
    switch (status.type()) {
        using enum fs::file_type;
    case directory:
        path = path / "";
        break;
    case not_found:
    case unknown:
    case none:
        return std::nullopt;
    default:
        break;
    }
    return path.generic_string();
}

bool path_within_dir(const std::string_view path, const std::string_view dir)
{
    static constexpr char portable_seperator = '/';
    static constexpr char preferred_separator = fs::path::preferred_separator;
    return path.starts_with(dir) && (dir.ends_with(portable_seperator) || dir.ends_with(preferred_separator)
            || path.size() == dir.size()
            || path[dir.size()] == portable_seperator || path[dir.size()] == preferred_separator);
}

}
