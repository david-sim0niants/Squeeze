#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "squeeze/compression/huffman_15.h"

#include "test_tools/generators/data_gen.h"
#include "test_tools/generators/prng.h"
#include "test_tools/generators/test_gen.h"
#include "test_common/test_data.h"
#include "test_common/test_on_data_input.h"

namespace squeeze::compression::testing {

using ::testing::Eq;
using ::testing::Pointwise;
using namespace squeeze::test_common;

class Huffman15Test : public ::testing::TestWithParam<const TestOnDataInput *> {};

TEST_P(Huffman15Test, EncodesDecodesData)
{
    std::vector<char> data = GetParam()->get_data();
    std::vector<char> buffer;
    auto bit_encoder = misc::make_bit_encoder(std::back_inserter(buffer));
    auto huffman15_encoder = Huffman15<>::make_encoder(bit_encoder);

    auto [in_it, ene] = huffman15_encoder.template encode_data<false>(data.begin(), data.end());
    EXPECT_TRUE(ene.successful()) << ene.report();
    EXPECT_EQ(in_it, data.end());
    EXPECT_EQ(bit_encoder.finalize(), 0);

    const double compression_ratio = data.size() / (double)buffer.size();
    EXPECT_GE(compression_ratio, 1.0);

    auto bit_decoder = misc::make_bit_decoder(buffer.begin(), buffer.end());
    auto huffman15_decoder = Huffman15<>::make_decoder(bit_decoder);

    std::vector<char> rest_data(data.size());
    auto [out_it, dee] = huffman15_decoder.template decode_data<false>(rest_data.begin(), rest_data.end());
    EXPECT_TRUE(dee.successful()) << dee.report();
    EXPECT_EQ(out_it, rest_data.end());

    ASSERT_THAT(rest_data, Pointwise(Eq(), data));
}

TEST_P(Huffman15Test, EncodeDecode)
{
    std::vector<char> data = GetParam()->get_data();
    std::vector<char> buffer;
    auto ene = std::get<2>(huffman15_encode(data.begin(), data.end(), std::back_inserter(buffer)));
    ASSERT_TRUE(ene.successful()) << ene.report();

    const double compression_ratio = data.size() / (double)buffer.size();
    EXPECT_GE(compression_ratio, 1.0);

    std::vector<char> rest_data(data.size());
    auto dee = std::get<2>(huffman15_decode(rest_data.begin(), rest_data.end(),
                           buffer.begin(), buffer.end()));
    EXPECT_TRUE(dee.successful()) << dee.report();

    ASSERT_THAT(data, Pointwise(Eq(), rest_data));
}

static const auto test_inputs = make_generated_test_on_data_inputs(128, 1234, 32);

INSTANTIATE_TEST_SUITE_P(Data, Huffman15Test, ::testing::ValuesIn(test_inputs.collect()));

}
