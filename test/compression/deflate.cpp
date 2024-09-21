#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "squeeze/compression/deflate.h"
#include "squeeze/compression/config.h"
#include "test_common/test_on_data_input.h"
#include "test_common/print_to.h"

namespace squeeze::compression::testing {

using ::testing::Eq;
using ::testing::Pointwise;
using namespace test_common;

using DeflateTestInput = std::tuple<const TestOnDataInput *, DeflateParams>;

class DeflateTest : public ::testing::TestWithParam<DeflateTestInput> {
};

TEST_P(DeflateTest, EncodeDecode)
{
    std::vector<char> data {std::get<0>(GetParam())->get_data()};
    DeflateParams deflate_params = std::get<1>(GetParam());
    deflate_params.header_bits = DeflateHeaderBits::FinalBlock | DeflateHeaderBits::DynamicHuffman;
    std::vector<char> buffer;

    auto [in_it, out_it, s] = deflate(deflate_params, data.begin(), data.end(), std::back_inserter(buffer));
    EXPECT_EQ(in_it, data.end());
    EXPECT_TRUE(s.successful()) << s.report();

    const double compression_ratio = data.size() / (double)buffer.size();
    EXPECT_GE(compression_ratio, 1.0);

    std::vector<char> rest_data (data.size());
    {
        auto [out_it, in_it, header_bits, s] =
            inflate(rest_data.begin(), rest_data.end(), buffer.begin(), buffer.end());
        EXPECT_EQ(out_it, rest_data.end());
        EXPECT_EQ(in_it, buffer.end());
        EXPECT_TRUE(s.successful()) << s.report();
    }

    ASSERT_THAT(rest_data, Pointwise(Eq(), data));
}

static const auto test_inputs = make_generated_test_on_data_inputs(16, 42, 16);

#define SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_DEFLATE_TEST(level) \
    INSTANTIATE_TEST_SUITE_P(Level_##level, DeflateTest, \
            ::testing::Combine(::testing::ValuesIn(test_inputs.collect()), \
                               ::testing::Values(get_deflate_params_for_level(level))))

SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_DEFLATE_TEST(0);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_DEFLATE_TEST(1);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_DEFLATE_TEST(2);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_DEFLATE_TEST(3);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_DEFLATE_TEST(4);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_DEFLATE_TEST(5);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_DEFLATE_TEST(6);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_DEFLATE_TEST(7);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_DEFLATE_TEST(8);

#undef SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_DEFLATE_TEST

}
