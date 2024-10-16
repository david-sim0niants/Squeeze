// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "test_tools/generators/data_gen.h"
#include "test_tools/generators/mockfs_gen.h"
#include "test_tools/generators/prng.h"

namespace squeeze::test_tools::generators {

namespace {

std::string gen_dirname(const PRNG& prng)
{
    constexpr int min_name_len = 4;
    constexpr int max_name_len = 8;
    return gen_alphanumeric_string(prng(min_name_len, max_name_len), prng) + mock::FileSystem::seperator;
}

std::string gen_regular_file_name(const PRNG& prng)
{
    constexpr int min_name_len = 4;
    constexpr int max_name_len = 8;
    constexpr int min_ext_len = 1;
    constexpr int max_ext_len = 3;
    return gen_alphanumeric_string(prng(min_name_len, max_name_len), prng) + '.' +
           gen_alphanumeric_string(prng(min_ext_len, max_ext_len), prng);
}

std::string gen_symlink_name(const PRNG& prng)
{
    constexpr int min_name_len = 4;
    constexpr int max_name_len = 8;
    return gen_alphanumeric_string(prng(min_name_len, max_name_len), prng);
}

EntryPermissions gen_permissions(const PRNG& prng)
{
    return static_cast<EntryPermissions>(
                prng(std::underlying_type_t<EntryPermissions>(),
                static_cast<std::underlying_type_t<EntryPermissions>>(EntryPermissions::All))
            );
}

mock::FileEntries gen_mockfs_entries(const PRNG& prng, int max_breadth, int depth)
{
    mock::FileEntries entries;
    if (depth == 0)
        return entries;
    int breadth = prng(1, max_breadth);
    while (breadth--) {
        std::string str = gen_dirname(prng);
        entries.get<mock::Directory>()[str] =
                std::make_shared<mock::Directory>(
                        gen_permissions(prng), gen_mockfs_entries(prng, max_breadth, depth - 1));
    }
    return entries;
}

mock::RegularFile gen_mock_regular_file(const PRNG& prng, const std::string_view data_seed)
{
    mock::RegularFile regular_file {gen_permissions(prng)};
    std::vector<char> data {gen_data(prng, data_seed)};
    regular_file.contents.write(data.data(), data.size());
    return regular_file;
}

std::shared_ptr<mock::Symlink> gen_mock_symlink(const PRNG& prng, std::string&& target)
{
    return std::make_shared<mock::Symlink>(gen_permissions(prng), std::move(target));
}

}

mock::FileSystem gen_mockfs(const std::string_view data_seed, const PRNG& prng)
{
    constexpr int nr_regular_files = 20;
    constexpr int nr_symlinks = 20;

    // generate initial mockfs structure
    mock::FileSystem mockfs {gen_mockfs_entries(prng, 3, 4)};

    // collect all its directories, subdirectories...
    std::vector<std::shared_ptr<mock::Directory>> directories;
    mockfs.list_recursively<mock::Directory>(
        [&directories](const std::string& path, std::shared_ptr<mock::Directory> directory)
        {
            directories.push_back(directory);
        }
    );

    // generate regular files
    for (int i = 0; i < nr_regular_files; ++i) {
        std::string file_name = gen_regular_file_name(prng);
        mock::Directory& dir = *directories[prng(std::size_t(0), directories.size() - 1)];
        dir.make<mock::RegularFile>(file_name, gen_mock_regular_file(prng, data_seed));
    }

    // generate symlinks
    int nr_symlinks_left = nr_symlinks;
    auto symlink_make = [&prng, &directories, &nr_symlinks_left](const std::string& path, auto& file)
    {
        if (!nr_symlinks_left)
            return;
        --nr_symlinks_left;

        directories[prng(std::size_t(0), directories.size() - 1)]->
            entries.symlinks[gen_symlink_name(prng)] = gen_mock_symlink(prng, std::string(path));
    };
    mockfs.list_recursively<mock::RegularFile, mock::Directory>(symlink_make, symlink_make);

    return mockfs;
}

}
