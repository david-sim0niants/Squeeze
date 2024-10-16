// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <sstream>
#include <utility>

#include "squeeze/compression/params.h"
#include "squeeze/encode.h"
#include "squeeze/decode.h"

#include "test_tools/generators/test_gen.h"

#include "test_common/test_data.h"
#include "test_common/test_on_data_input.h"
#include "test_common/print_to.h"

namespace squeeze::testing {

using ::testing::Eq;
using ::testing::Pointwise;
using namespace test_common;
using compression::CompressionParams;
using compression::CompressionMethod;

using EncodeDecodeTestInput = std::tuple<const TestOnDataInput *, compression::CompressionParams>;

class EncodeDecodeTest : public ::testing::TestWithParam<EncodeDecodeTestInput> {};

TEST_P(EncodeDecodeTest, EncodeDecode)
{
    const CompressionParams compression = std::get<1>(GetParam());

    std::vector<char> data {std::get<0>(GetParam())->get_data()};
    std::stringstream content;
    const std::size_t content_size = data.size();
    content.write(data.data(), content_size);

    std::stringstream compressed;
    EncodeStat en_stat = encode(content, content_size, compressed, compression);
    EXPECT_TRUE(en_stat.successful()) << en_stat.report();

    const std::size_t compressed_size = compressed.tellp();

    const double compression_ratio = content_size / (double)compressed_size;
    EXPECT_GE(compression_ratio, 1.0);

    std::stringstream restored_content;
    DecodeStat de_stat = decode(restored_content, compressed_size, compressed, compression);
    EXPECT_TRUE(de_stat.successful()) << de_stat.report();

    ASSERT_THAT(restored_content.view(), Pointwise(Eq(), content.view()));
}

static const auto test_inputs = make_generated_test_on_data_inputs(128, 1234, 16);

#define SQUEEZE_TESTING_INSTANTIATE_COMPRESSION_TEST(method, level) \
    INSTANTIATE_TEST_SUITE_P(method##_##level, EncodeDecodeTest, \
        ::testing::Combine(::testing::ValuesIn(test_inputs.collect()), \
                           ::testing::Values(CompressionParams{CompressionMethod::method, level})))

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
