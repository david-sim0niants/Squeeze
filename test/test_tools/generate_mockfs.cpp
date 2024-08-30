#include "generate_mockfs.h"
#include "generate_mock_contents.h"
#include "random.h"

namespace squeeze::testing::tools {

std::string generate_random_dirname(const Random<int>& prng)
{
    constexpr int min_name_len = 4;
    constexpr int max_name_len = 8;
    return generate_random_string(prng, prng(min_name_len, max_name_len)) + MockFileSystem::seperator;
}

std::string generate_random_regular_file_name(const Random<int>& prng)
{
    constexpr int min_name_len = 4;
    constexpr int max_name_len = 8;
    constexpr int min_ext_len = 1;
    constexpr int max_ext_len = 3;
    return generate_random_string(prng, prng(min_name_len, max_name_len)) + '.' +
           generate_random_string(prng, prng(min_ext_len, max_ext_len));
}

std::string generate_random_symlink_name(const Random<int>& prng)
{
    constexpr int min_name_len = 4;
    constexpr int max_name_len = 8;
    return generate_random_string(prng, prng(min_name_len, max_name_len));
}

static EntryPermissions generate_random_permissions(const Random<int>& prng)
{
    return static_cast<EntryPermissions>(prng(0, static_cast<int>(EntryPermissions::All)));
}

static MockDirectory generate_mockfs_entries(const Random<int>& prng, int max_breadth, int depth)
{

    MockDirectory entry(generate_random_permissions(prng));
    if (depth == 0)
        return entry;
    int breadth = prng(1, max_breadth);
    while (breadth--) {
        std::string str = generate_random_dirname(prng);
        entry.entries.get<MockDirectory>()[str] =
                std::make_shared<MockDirectory>(generate_mockfs_entries(prng, max_breadth, depth - 1));
    }

    return entry;
}

static std::shared_ptr<MockRegularFile> generate_mock_regular_file(
        const Random<int>& prng, const std::string_view content_seed,
        const MockContentsGeneratorParams& params)
{
    std::shared_ptr<MockRegularFile> regular_file = std::make_shared<MockRegularFile>(
            generate_random_permissions(prng));
    generate_mock_contents(params, prng, content_seed, regular_file->contents);
    return regular_file;
}

static std::shared_ptr<MockSymlink> generate_mock_symlink(const Random<int>& prng, std::string&& target)
{
    return std::make_shared<MockSymlink>(generate_random_permissions(prng), std::move(target));
}

MockFileSystem generate_mockfs(const Random<int>& prng, const std::string_view content_seed)
{
    constexpr int nr_regular_files = 20;
    constexpr int nr_symlinks = 20;

    MockFileSystem mockfs = generate_mockfs_entries(prng, 3, 4);

    std::vector<std::shared_ptr<MockDirectory>> directories;
    mockfs.list_recursively<MockDirectory>(
        [&directories](const std::string& path, std::shared_ptr<MockDirectory> directory)
        {
            directories.push_back(directory);
        }
    );

    MockContentsGeneratorParams params = {
        .size_limit = 2 << 20,
        .min_nr_reps = 1,
        .max_nr_reps = (2 << 20) / content_seed.size() + 1,
        .noise_probability = 32,
        .flags = MockContentsGeneratorParams::ApplyNoise
               | MockContentsGeneratorParams::RandomizedRange
               | MockContentsGeneratorParams::RandomizedRepCount,
    };

    std::vector<std::shared_ptr<MockRegularFile>> regular_files;

    for (int i = 0; i < nr_regular_files; ++i) {
        std::string file_name = generate_random_regular_file_name(prng);
        auto& entries = directories[prng(0, directories.size() - 1)]->entries;
        regular_files.push_back(
                entries.regular_files[file_name] = generate_mock_regular_file(prng, content_seed, params));
    }

    int nr_symlinks_left = nr_symlinks;
    auto symlink_make = [&prng, &directories, &nr_symlinks_left](const std::string& path, auto& file)
    {
        if (!nr_symlinks_left)
            return;
        --nr_symlinks_left;

        directories[prng(0, directories.size() - 1)]->
            entries.symlinks[generate_random_symlink_name(prng)] = generate_mock_symlink(prng, std::string(path));
    };
    mockfs.list_recursively<MockRegularFile, MockDirectory>(symlink_make, symlink_make);

    return mockfs;
}

}
