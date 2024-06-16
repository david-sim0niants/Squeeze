#include "squeeze/squeeze.h"

#include <gtest/gtest.h>

#include <sstream>

#include "test_tools/generate_mockfs.h"
#include "test_tools/mock_entry_input.h"
#include "test_tools/mock_entry_output.h"

namespace squeeze::testing {

static void observe_mockfs(const tools::MockFileSystem& mockfs)
{
    auto observer =
        [](std::string& path, auto file)
        {
            std::cerr << utils::stringify(file->get_attributes())
                      << "    " << path << std::endl;
            return 0;
        };
    auto symlink_observer =
        [](std::string& path, std::shared_ptr<tools::MockSymlink> symlink)
        {
            std::cerr << utils::stringify(symlink->get_attributes())
                      << "    " << path << " -> " << symlink->target << '\n';
            return 0;
        };

    mockfs.list_recursively<
        tools::MockRegularFile, tools::MockSymlink, tools::MockDirectory>(
                observer, symlink_observer, observer);
}

template<typename MockFile>
static void test_mockfs_entries(
        const tools::MockFileEntriesMap<MockFile>& original,
        const tools::MockFileEntriesMap<MockFile>& restored);

static void test_mockfs(const tools::MockFileSystem& original, const tools::MockFileSystem& restored)
{
    tools::MockFileEntries original_flatten = tools::flatten_mockfs(original);
    tools::MockFileEntries restored_flatten = tools::flatten_mockfs(restored);

    test_mockfs_entries(
            original_flatten.get<tools::MockRegularFile>(),
            restored_flatten.get<tools::MockRegularFile>());
    test_mockfs_entries(
            original_flatten.get<tools::MockDirectory>(),
            restored_flatten.get<tools::MockDirectory>());
    test_mockfs_entries(
            original_flatten.get<tools::MockSymlink>(),
            restored_flatten.get<tools::MockSymlink>());
}

template<typename MockFile>
static void test_mock_files(const MockFile& original, const MockFile& restored) {}

template<typename MockFile>
static void test_mockfs_entries(
        const tools::MockFileEntriesMap<MockFile>& original,
        const tools::MockFileEntriesMap<MockFile>& restored)
{
    EXPECT_EQ(original.size(), restored.size());
    for (const auto& [path, file] : original) {
        auto it = restored.find(path);
        EXPECT_TRUE(it != restored.end());
        if (it == restored.end())
            continue;
        test_mock_files(*it->second, *file);
    }
}

static void test_mock_attributes(EntryAttributes original, EntryAttributes restored)
{
    EXPECT_EQ(original.type, restored.type);
    EXPECT_EQ(original.permissions, restored.permissions);
}

template<> void test_mock_files<tools::MockRegularFile>(
        const tools::MockRegularFile& original, const tools::MockRegularFile& restored)
{
    test_mock_attributes(original.get_attributes(), restored.get_attributes());
    EXPECT_EQ(original.contents.view(), restored.contents.view());
}

template<> void test_mock_files<tools::MockDirectory>(
        const tools::MockDirectory& original, const tools::MockDirectory& restored)
{
    test_mock_attributes(original.get_attributes(), restored.get_attributes());
}

template<> void test_mock_files<tools::MockSymlink>(
        const tools::MockSymlink& original, const tools::MockSymlink& restored)
{
    test_mock_attributes(original.get_attributes(), restored.get_attributes());
    EXPECT_EQ(original.target, restored.target);
}


static void encode_mockfs(Squeeze& squeeze, const tools::MockFileSystem& original_mockfs,
        CompressionMethod compression_method, int compression_level)
{
    std::deque<Error<Writer>> write_errors;
    auto append_file =
        [&squeeze, &write_errors, compression_method, compression_level]
        (const std::string& path, auto file)
        {
            write_errors.emplace_back();
            squeeze.will_append<tools::MockEntryInput>(write_errors.back(),
                    std::string(path), compression_method, compression_level, file);
        };
    original_mockfs.list_recursively<tools::MockRegularFile, tools::MockDirectory, tools::MockSymlink>(
            append_file, append_file, append_file);
    squeeze.update();

    for (const auto& err : write_errors)
        EXPECT_FALSE(err.failed()) << err.report();
}

static void decode_mockfs(Squeeze& squeeze, tools::MockFileSystem& restored_mockfs)
{
    for (auto it = squeeze.begin(); it != squeeze.end(); ++it) {
        tools::MockEntryOutput mock_entry_output(restored_mockfs);
        auto err = squeeze.extract(it, mock_entry_output);
        EXPECT_FALSE(err.failed()) << err.report();
    }
}


struct TestArgs {
    int prng_seed;
    const char *file_content_seed;
    CompressionMethod compression_method;
    int compression_level;
};

class SqueezeTest : public ::testing::Test, public ::testing::WithParamInterface<TestArgs> {
protected:
    SqueezeTest() :
        prng(GetParam().prng_seed),
        content(std::ios_base::binary | std::ios_base::in | std::ios_base::out),
        squeeze(content)
    {}

    inline tools::MockFileSystem generate_mockfs()
    {
        return tools::generate_mockfs(prng, GetParam().file_content_seed);
    }

    inline void encode_mockfs(const tools::MockFileSystem& original_mockfs)
    {
        testing::encode_mockfs(squeeze, original_mockfs,
                GetParam().compression_method, GetParam().compression_level);
    }

    inline void decode_mockfs(tools::MockFileSystem& restored_mockfs)
    {
        testing::decode_mockfs(squeeze, restored_mockfs);
    }

    static inline void present_generated_mockfs(const tools::MockFileSystem& generated_mockfs)
    {
        std::cerr << "\033[32m" << "Generated mock filesystem:" << "\033[0m" << '\n';
        observe_mockfs(generated_mockfs);
    }

    static inline void present_recreated_mockfs(const tools::MockFileSystem& recreated_mockfs)
    {
        std::cerr << "\033[32m\n" << "Recreated mock filesystem:" << "\033[0m" << '\n';
        observe_mockfs(recreated_mockfs);
    }

    tools::Random<int> prng;
    std::stringstream content;
    Squeeze squeeze;
};

TEST_P(SqueezeTest, WriteRead)
{
    tools::MockFileSystem generated_mockfs = generate_mockfs(), recreated_mockfs;

    std::cerr << '\n';
    present_generated_mockfs(generated_mockfs);

    encode_mockfs(generated_mockfs);
    decode_mockfs(recreated_mockfs);

    present_recreated_mockfs(recreated_mockfs);
    std::cerr << '\n';

    test_mockfs(generated_mockfs, recreated_mockfs);
}

TEST_P(SqueezeTest, WriteUpdateRead)
{
    tools::MockFileSystem generated_mockfs = generate_mockfs(), recreated_mockfs;

    encode_mockfs(generated_mockfs);
    generated_mockfs.update(generate_mockfs());

    std::cerr << '\n';
    present_generated_mockfs(generated_mockfs);

    encode_mockfs(generated_mockfs);
    decode_mockfs(recreated_mockfs);

    present_recreated_mockfs(recreated_mockfs);
    std::cerr << '\n';

    test_mockfs(generated_mockfs, recreated_mockfs);
}

static constexpr int prng_seed = 1234;
static constexpr char file_content_seed[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec tristique dictum ex, vitae egestas elit aliquet ac. Pellentesque consectetur sapien quis purus euismod, vel efficitur nibh lobortis. In hac habitasse platea dictumst. Proin aliquam vulputate molestie. Morbi vehicula facilisis enim, id maximus tortor mollis at. Aenean vitae consequat turpis. Proin efficitur, dui ac commodo molestie, sapien lectus dignissim lectus, eget condimentum lacus lorem nec odio. Pellentesque convallis nulla eget malesuada consectetur. Fusce ullamcorper, massa non sagittis eleifend, arcu eros euismod orci, nec malesuada mauris mauris id risus. Vestibulum ullamcorper ac mi et fringilla. Proin sed lectus tincidunt, tristique enim in, interdum elit. Duis ac tincidunt diam, a vehicula leo. Ut luctus, ex eu ultrices vehicula, lorem enim dignissim tellus, ut congue nisi velit sit amet ante. Cras nec sagittis justo.\n";

INSTANTIATE_TEST_SUITE_P(AnyCompression, SqueezeTest, ::testing::Values(
            TestArgs {prng_seed, file_content_seed, CompressionMethod::None, 0}
        )
    );


}
