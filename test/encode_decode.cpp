#include <gtest/gtest.h>

#include <sstream>
#include <utility>

#include "squeeze/compression/params.h"
#include "squeeze/encoder.h"
#include "squeeze/decoder.h"
#include "test_tools/generate_mock_contents.h"

namespace squeeze::testing {

struct EncodeDecodeTestInput {
    EncodeDecodeTestInput(int prng_seed, std::string_view content_seed,
            compression::CompressionParams compression)
        : prng_seed(prng_seed), content_seed(content_seed), compression(compression)
    {
    }

    std::pair<std::stringstream, std::size_t> gen_contents() const
    {
        using namespace ::squeeze::testing::tools;
        Random<int> prng(prng_seed);
        const MockContentsGeneratorParams params = {
            .size_limit = 2 << 20,
            .min_nr_reps = 1,
            .max_nr_reps = (2 << 20) / content_seed.size() + 1,
            .flags = MockContentsGeneratorParams::RandomizedRange
                   | MockContentsGeneratorParams::RandomizedRepCount,
        };
        std::stringstream contents;
        generate_mock_contents(params, prng, content_seed, contents);
        return std::make_pair(std::move(contents), contents.tellp());
    }

    int prng_seed;
    std::string_view content_seed;
    compression::CompressionParams compression;
};

class EncodeDecodeTest : public ::testing::TestWithParam<EncodeDecodeTestInput> {
};

TEST_P(EncodeDecodeTest, EncodeDecode)
{
    const compression::CompressionParams compression = GetParam().compression;

    auto [content, content_size] = GetParam().gen_contents();

    std::stringstream compressed;
    EXPECT_TRUE(encode(content, content_size, compressed, compression).successful());

    const std::size_t compressed_size = compressed.tellp();

    const double compression_ratio = content_size / (double)compressed_size;
    std::cerr << "\033[32m" "Compression ratio: " << compression_ratio
        << " | Compressed " << 100 * (1 - 1.0 / compression_ratio) << '%' << "\033[0m\n";

    std::stringstream restored_content;
    EXPECT_TRUE(decode(restored_content, compressed_size, compressed, compression).successful());

    EXPECT_EQ(content.view().size(), restored_content.view().size());
    EXPECT_EQ(content.view(), restored_content.view());
}

static std::string_view content_seed = "Sed non lectus congue, ultricies elit vel, volutpat orci. Sed sed mauris at mauris laoreet varius. Vestibulum in erat tincidunt, sodales mauris id, rutrum orci. Suspendisse suscipit, tellus eu vehicula finibus, felis lacus consectetur ligula, consequat tempor est ipsum in quam. Aliquam venenatis, nisl id porttitor suscipit, urna est dignissim sapien, nec sollicitudin erat nisi nec lorem. Ut non lacus sem. Vivamus mollis faucibus sollicitudin. Mauris nec risus ut metus finibus eleifend eu non eros. Ut at placerat sem. Etiam eget leo eget odio lobortis sollicitudin non sit amet ligula. Proin euismod justo non posuere euismod. Nullam pretium, nulla a vulputate dignissim, leo dolor rhoncus nunc, eu lobortis erat purus at lacus. Donec vel elit non nibh bibendum tincidunt sed vitae velit. Duis sem urna, euismod et dapibus sed, tempus ac arcu. Nam finibus finibus porttitor. Mauris cursus tortor vehicula, gravida tellus quis, aliquam nulla.";

std::vector<EncodeDecodeTestInput> gen_test_inputs(const compression::CompressionParams& compression)
{
    std::vector<EncodeDecodeTestInput> inputs;
    constexpr std::size_t nr_tests_per_compression_info = 16;
    for (std::size_t i = 0; i < nr_tests_per_compression_info; ++i)
        inputs.emplace_back(1234 + i, content_seed, compression);
    return inputs;
}

#define SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(method, level) \
    INSTANTIATE_TEST_SUITE_P(method##_##level, EncodeDecodeTest, \
            ::testing::ValuesIn(gen_test_inputs({compression::CompressionMethod::method, level})))

SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(None,    0);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 1);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 2);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 3);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 4);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 5);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 6);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 7);
SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(Huffman, 8);

#undef SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST

}
