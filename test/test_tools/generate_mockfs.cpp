#include "generate_mockfs.h"
#include "random.h"

namespace squeeze::testing::tools {

const std::string mockfs_seed = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec tristique dictum ex, vitae egestas elit aliquet ac. Pellentesque consectetur sapien quis purus euismod, vel efficitur nibh lobortis. In hac habitasse platea dictumst. Proin aliquam vulputate molestie. Morbi vehicula facilisis enim, id maximus tortor mollis at. Aenean vitae consequat turpis. Proin efficitur, dui ac commodo molestie, sapien lectus dignissim lectus, eget condimentum lacus lorem nec odio. Pellentesque convallis nulla eget malesuada consectetur. Fusce ullamcorper, massa non sagittis eleifend, arcu eros euismod orci, nec malesuada mauris mauris id risus. Vestibulum ullamcorper ac mi et fringilla. Proin sed lectus tincidunt, tristique enim in, interdum elit. Duis ac tincidunt diam, a vehicula leo. Ut luctus, ex eu ultrices vehicula, lorem enim dignissim tellus, ut congue nisi velit sit amet ante. Cras nec sagittis justo.\n";

static std::string generate_random_string(const Random<int>& prng, size_t size)
{
    std::string str;
    str.resize(size);

    constexpr char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
    constexpr size_t alphanum_max_idx = sizeof(alphanum) / sizeof(alphanum[0]) - 2;

    for (size_t i = 0; i < size; ++i)
        str[i] = alphanum[prng(0, alphanum_max_idx)];
    return str;
}

std::string generate_random_dirname(const Random<int>& prng)
{
    constexpr int min_name_len = 4;
    constexpr int max_name_len = 8;
    return generate_random_string(prng, prng(min_name_len, max_name_len)) + '/';
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

struct MockRegularFileGenerateParams {
    size_t size_limit;
    int min_nr_reps;
    int max_nr_reps;
    unsigned char noise_probability = 25;
    enum {
        NoneFlags = 0,
        ApplyNoise = 1,
        RandomizedRepCount = 2,
        RandomizedRange = 4,
    }; int flags;
};

static void generate_mock_regular_file_contents(const std::string_view seed, std::stringstream& contents,
        const MockRegularFileGenerateParams& params)
{
    Random<size_t> prng;
    int nr_reps = (params.flags & MockRegularFileGenerateParams::RandomizedRepCount) ?
            prng(params.min_nr_reps, params.max_nr_reps) : params.min_nr_reps;
    size_t curr_size = 0;
    while (nr_reps-- && curr_size < params.size_limit) {
        size_t pos = 0, len = seed.size();
        if (params.flags & MockRegularFileGenerateParams::RandomizedRange) {
            pos = prng(0, len - 1);
            len = prng(0, len - 1 - pos);
        }
        len = std::min(len, params.size_limit - curr_size);

        std::string data {seed.substr(pos, len)};
        if (params.flags & MockRegularFileGenerateParams::ApplyNoise) {
            for (char& c : data) {
                c = static_cast<unsigned char>(prng(0, 256)) <= params.noise_probability ?
                        static_cast<char>(prng(0, 256)) : c;
            }
        }
        contents.write(data.data(), data.size());
        curr_size += len;
    }
}

static std::shared_ptr<MockRegularFile> generate_mock_regular_file(const std::string_view seed,
        const MockRegularFileGenerateParams& params)
{
    std::shared_ptr<MockRegularFile> regular_file = std::make_shared<MockRegularFile>(
            generate_random_permissions(Random<int>()));
    generate_mock_regular_file_contents(seed, regular_file->contents, params);
    return regular_file;
}

static std::shared_ptr<MockSymlink> generate_mock_symlink(const Random<int>& prng, std::string&& target)
{
    return std::make_shared<MockSymlink>(generate_random_permissions(prng), std::move(target));
}

MockFileSystem generate_mockfs(const std::string_view seed)
{
    constexpr int prng_seed = 1234;
    constexpr int nr_regular_files = 20;
    constexpr int nr_symlinks = 20;

    Random<int> prng(prng_seed);

    MockFileSystem mockfs = generate_mockfs_entries(prng, 3, 4);

    std::vector<std::shared_ptr<MockDirectory>> directories;
    mockfs.list_recursively<MockDirectory>(
        [&directories](const std::string& path, std::shared_ptr<MockDirectory> directory)
        {
            directories.push_back(directory);
        }
    );

    MockRegularFileGenerateParams params = {
        .size_limit = 2 << 20,
        .min_nr_reps = 20,
        .max_nr_reps = 20000,
        .noise_probability = 25,
        .flags = MockRegularFileGenerateParams::ApplyNoise
               | MockRegularFileGenerateParams::RandomizedRange
               | MockRegularFileGenerateParams::RandomizedRepCount,
    };

    std::vector<std::shared_ptr<MockRegularFile>> regular_files;

    for (int i = 0; i < nr_regular_files; ++i) {
        std::string file_name = generate_random_regular_file_name(prng);
        auto& entries = directories[prng(0, directories.size() - 1)]->entries;
        regular_files.push_back(
                entries.regular_files[file_name] = generate_mock_regular_file(seed, params));
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
