#include "squeeze/utils/fs.h"

#include <filesystem>
#include <system_error>

namespace squeeze::utils {

static std::error_code create_directories(const std::filesystem::path& path);

std::variant<std::fstream, ErrorCode>
    make_regular_file(std::string_view path_str, EntryPermissions entry_perms)
{
    std::filesystem::path path(path_str);
    std::error_code ec = utils::create_directories(path.parent_path());
    if (ec)
        return ec;

    std::fstream file(path, std::ios_base::binary | std::ios_base::in | std::ios_base::out);
    if (!file)
        return std::make_error_code(static_cast<std::errc>(errno));

    ErrorCode ec_ = set_permissions(path, entry_perms);
    if (ec_)
        return ec_;
    return file;
}

std::variant<std::ofstream, ErrorCode>
    make_regular_file_out(std::string_view path_str, EntryPermissions entry_perms)
{
    std::filesystem::path path(path_str);
    std::error_code ec = utils::create_directories(path.parent_path());
    if (ec)
        return ec;

    std::ofstream file(path, std::ios_base::binary | std::ios_base::out);
    if (!file)
        return std::make_error_code(static_cast<std::errc>(errno));

    ErrorCode ec_ = set_permissions(path, entry_perms);
    if (ec_)
        return ec_;
    return file;
}

ErrorCode make_directory(std::string_view path_str, EntryPermissions entry_perms)
{
    std::filesystem::path path(path_str);
    std::error_code ec;

    std::filesystem::create_directories(path, ec);
    if (ec)
        return ec;

    return set_permissions(path, entry_perms);
}

ErrorCode make_symlink(std::string_view path_str, std::string_view link_to,
        EntryPermissions entry_perms)
{
    std::filesystem::path path(path_str);
    std::error_code ec;

    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
        return ec;

    std::filesystem::create_symlink(std::filesystem::path(link_to), path, ec);
    if (ec)
        return ec;

    return success;
}

ErrorCode set_permissions(const std::filesystem::path &path, EntryPermissions entry_perms)
{
    std::error_code ec;
    std::filesystem::perms perms;
    convert(entry_perms, perms);
    std::filesystem::permissions(path, perms, ec);
    if (ec)
        return ec;
    return success;
}

void convert(const EntryPermissions& from, std::filesystem::perms& to)
{
    using enum EntryPermissions;
    using enum std::filesystem::perms;
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

void convert(const std::filesystem::perms& from, EntryPermissions& to)
{
    using enum std::filesystem::perms;
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

void convert(const EntryType& from, std::filesystem::file_type& to)
{
    using enum EntryType;
    using enum std::filesystem::file_type;
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

void convert(const std::filesystem::file_type& from, EntryType& to)
{
    using enum EntryType;
    using enum std::filesystem::file_type;
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

static std::error_code create_directories(const std::filesystem::path& path)
{
    std::error_code ec;
    if (path.empty())
        return ec;
    std::filesystem::create_directories(path, ec);
    return ec;
}

}
