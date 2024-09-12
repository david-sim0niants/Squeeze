#include "test_tools/mock/fs.h"

namespace squeeze::test_tools::mock {

std::shared_ptr<Directory> Directory::make_directories(std::string_view path)
{
    std::shared_ptr<Directory> curdir_shared = nullptr;
    Directory *curdir = this;
    while (!path.empty()) {
        size_t i_sep = path.find(seperator);
        if (i_sep == std::string_view::npos)
            break;

        auto& curdir_shared_ref = curdir->entries.directories[std::string(path.substr(0, i_sep + 1))];
        if (!curdir_shared_ref)
            curdir_shared_ref = std::make_shared<Directory>();

        curdir_shared = curdir_shared_ref;
        curdir = curdir_shared.get();
        path = path.substr(i_sep + 1);
    }
    return curdir_shared;
}

FileEntries flatten_fs(const FileSystem &mockfs)
{
    FileEntries flatten_entries;
    auto handle_file = [&flatten_entries](const std::string& path, auto file)
    {
        flatten_entries.get<typename decltype(file)::element_type>()[path] = file;
    };
    mockfs.list_recursively<RegularFile, Directory, Symlink>(
            handle_file, handle_file, handle_file);
    return flatten_entries;
}

}
