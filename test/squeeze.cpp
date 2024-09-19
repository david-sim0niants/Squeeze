#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <sstream>

#include "squeeze/squeeze.h"

#include "test_tools/generators/mockfs_gen.h"
#include "test_tools/mock/entry_input.h"
#include "test_tools/mock/entry_output.h"
#include "test_common/test_data.h"
#include "test_common/print_to.h"

namespace squeeze::testing {

using ::testing::Eq;
using ::testing::Pointwise;
using namespace test_tools;
using namespace test_common;

static void observe_mockfs(const mock::FileSystem& mockfs, std::ostream& os)
{
    auto observer =
        [&os](std::string& path, auto file)
        {
            os << utils::stringify(file->get_attributes()) << "    " << path << std::endl;
            return 0;
        };
    auto symlink_observer =
        [&os](std::string& path, std::shared_ptr<mock::Symlink> symlink)
        {
            os << utils::stringify(symlink->get_attributes())
               << "    " << path << " -> " << symlink->target << '\n';
            return 0;
        };

    mockfs.list_recursively<
        mock::RegularFile, mock::Symlink, mock::Directory>(
                observer, symlink_observer, observer);
}

template<typename MockFile>
static void test_mockfs_entries(
        const mock::FileEntriesMap<MockFile>& original,
        const mock::FileEntriesMap<MockFile>& restored);

static void test_mockfs(const mock::FileSystem& original, const mock::FileSystem& restored)
{
    mock::FileEntries original_flatten = mock::flatten_fs(original);
    mock::FileEntries restored_flatten = mock::flatten_fs(restored);

    test_mockfs_entries(
            original_flatten.get<mock::RegularFile>(),
            restored_flatten.get<mock::RegularFile>());
    test_mockfs_entries(
            original_flatten.get<mock::Directory>(),
            restored_flatten.get<mock::Directory>());
    test_mockfs_entries(
            original_flatten.get<mock::Symlink>(),
            restored_flatten.get<mock::Symlink>());
}

template<typename MockFile>
static void test_mock_files(const MockFile& original, const MockFile& restored, std::string_view path) {}

template<typename MockFile>
static void test_mockfs_entries(
        const mock::FileEntriesMap<MockFile>& original,
        const mock::FileEntriesMap<MockFile>& restored)
{
    EXPECT_EQ(original.size(), restored.size());
    for (const auto& [path, file] : original) {
        auto it = restored.find(path);
        EXPECT_TRUE(it != restored.end());
        if (it == restored.end())
            continue;
        test_mock_files(*file, *it->second, path);
    }
}

static void test_mock_attributes(EntryAttributes original, EntryAttributes restored)
{
    EXPECT_EQ(original.type, restored.type);
    EXPECT_EQ(original.permissions, restored.permissions);
}

template<> void test_mock_files<mock::RegularFile>(
        const mock::RegularFile& original, const mock::RegularFile& restored, std::string_view path)
{
    test_mock_attributes(original.get_attributes(), restored.get_attributes());
    EXPECT_THAT(restored.contents.view(), Pointwise(Eq(), original.contents.view()))
        << '\'' << path << '\'' << " symlink didn't restore properly";
}

template<> void test_mock_files<mock::Directory>(
        const mock::Directory& original, const mock::Directory& restored, std::string_view path)
{
    test_mock_attributes(original.get_attributes(), restored.get_attributes());
}

template<> void test_mock_files<mock::Symlink>(
        const mock::Symlink& original, const mock::Symlink& restored, std::string_view path)
{
    test_mock_attributes(original.get_attributes(), restored.get_attributes());
    EXPECT_THAT(restored.target, Pointwise(Eq(), original.target))
        << '\'' << path << '\'' << " symlink didn't restore properly";
}


static void encode_mockfs(Squeeze& squeeze, const mock::FileSystem& original_mockfs,
        const CompressionParams& compression)
{
    std::deque<Error<Writer>> write_errors;
    auto append_file =
        [&squeeze, &write_errors, &compression]
        (const std::string& path, auto file)
        {
            write_errors.emplace_back();
            squeeze.will_append<mock::EntryInput>(write_errors.back(),
                    std::string(path), compression, file);
        };
    original_mockfs.list_recursively<mock::RegularFile, mock::Directory, mock::Symlink>(
            append_file, append_file, append_file);
    EXPECT_TRUE(squeeze.update());

    for (const auto& err : write_errors)
        EXPECT_FALSE(err.failed()) << err.report();
}

static void decode_mockfs(Squeeze& squeeze, mock::FileSystem& restored_mockfs)
{
    for (auto it = squeeze.begin(); it != squeeze.end(); ++it) {
        mock::EntryOutput mock_entry_output(restored_mockfs);
        auto err = squeeze.extract(it, mock_entry_output);
        EXPECT_FALSE(err.failed()) << err.report();
    }
}

struct TestInput {
    int prng_seed;
    CompressionParams compression;
};

void PrintTo(const TestInput& test_input, std::ostream *os)
{
    *os << "Squeeze test input: {prng_seed=" << test_input.prng_seed << ", compression=";
    PrintTo(test_input.compression, os);
    *os << '}';
}

class SqueezeTest : public ::testing::TestWithParam<TestInput> {
protected:
    SqueezeTest() :
        prng(GetParam().prng_seed),
        content(std::ios_base::binary | std::ios_base::in | std::ios_base::out),
        squeeze(content)
    {}

    inline mock::FileSystem generate_mockfs()
    {
        return generators::gen_mockfs(TestData::get_data_seed(), prng);
    }

    inline void encode_mockfs(const mock::FileSystem& original_mockfs)
    {
        content.seekp(0, std::ios_base::beg);
        testing::encode_mockfs(squeeze, original_mockfs, GetParam().compression);
        if (content.tellp() < content.view().size())
            content.str(std::string(content.view().substr(0, content.tellp())));
    }

    inline void decode_mockfs(mock::FileSystem& restored_mockfs)
    {
        testing::decode_mockfs(squeeze, restored_mockfs);
    }

    inline void assert_if_corrupted()
    {
        content.seekg(0, std::ios_base::end);
        std::streamsize content_size = content.tellg();
        ASSERT_FALSE(squeeze.is_corrupted()) << "content_size=" << content_size;
        content.seekg(0, std::ios_base::beg);
    }

    static inline void present_generated_mockfs(const mock::FileSystem& generated_mockfs, std::ostream& os)
    {
        os << "\033[32m" "Generated mock filesystem:" << "\033[0m\n";
        observe_mockfs(generated_mockfs, os);
    }

    static inline void present_recreated_mockfs(const mock::FileSystem& recreated_mockfs, std::ostream& os)
    {
        os << "\033[32m" "Recreated mock filesystem:" << "\033[0m\n";
        observe_mockfs(recreated_mockfs, os);
    }

    generators::PRNG prng;
    std::stringstream content;
    Squeeze squeeze;
};

TEST_P(SqueezeTest, WriteRead)
{
    mock::FileSystem generated_mockfs = generate_mockfs(), recreated_mockfs;

    encode_mockfs(generated_mockfs);
    assert_if_corrupted();
    decode_mockfs(recreated_mockfs);

    test_mockfs(generated_mockfs, recreated_mockfs);

    if (HasFailure()) {
        present_generated_mockfs(generated_mockfs, std::cerr);
        std::cerr << "\033[31m" "didn't match to the\n" << "\033[0m";
        present_recreated_mockfs(recreated_mockfs, std::cerr);
    }
}

TEST_P(SqueezeTest, WriteUpdateRead)
{
    mock::FileSystem generated_mockfs = generate_mockfs(), recreated_mockfs;

    encode_mockfs(generated_mockfs);
    assert_if_corrupted();
    generated_mockfs.update(generate_mockfs());

    encode_mockfs(generated_mockfs);
    assert_if_corrupted();
    decode_mockfs(recreated_mockfs);

    test_mockfs(generated_mockfs, recreated_mockfs);

    if (HasFailure()) {
        present_generated_mockfs(generated_mockfs, std::cerr);
        std::cerr << "didn't match to the\n";
        present_recreated_mockfs(recreated_mockfs, std::cerr);
    }
}

static constexpr int prng_seed = 1234;

#define SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(method, level) \
    INSTANTIATE_TEST_SUITE_P(method##_##level, SqueezeTest, \
            ::testing::Values(TestInput {prng_seed, {compression::CompressionMethod::method, level}}))

SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(None,    0);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 1);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 2);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 3);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 4);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 5);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 6);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 7);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 8);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Deflate, 0);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Deflate, 1);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Deflate, 2);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Deflate, 3);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Deflate, 4);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Deflate, 5);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Deflate, 6);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Deflate, 7);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Deflate, 8);

#undef SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST

}
