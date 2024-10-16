// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "squeeze/compression/lz77.h"
#include "squeeze/compression/deflate_lz77.h"
#include "squeeze/compression/config.h"
#include "squeeze/utils/overloaded.h"

#include "test_tools/generators/test_gen.h"
#include "test_common/test_on_data_input.h"
#include "test_common/print_to.h"

namespace squeeze::compression::testing {

using ::testing::Eq;
using ::testing::Pointwise;
using namespace test_common;

using LZ77TestInput = std::tuple<const TestOnDataInput *, LZ77EncoderParams>;

class LZ77Test : public ::testing::TestWithParam<LZ77TestInput> {
};

TEST_P(LZ77Test, EncodeDecodeTokens)
{
    std::vector<char> data {std::get<0>(GetParam())->get_data()};
    auto encoder = LZ77<>::make_encoder(std::get<1>(GetParam()), data.begin(), data.end());
    std::vector<char> rest_data;
    auto decoder = LZ77<>::make_decoder(std::back_inserter(rest_data));

    while (true) {
        LZ77<>::Token token = encoder.encode_once();
        if (token.get_type() == LZ77<>::Token::None)
            break;
        const std::size_t nr_syms_encoded = token.get_nr_syms_within();

        std::size_t rest_data_i = rest_data.size();
        auto s = decoder.decode_once(token);
        EXPECT_TRUE(s.successful()) << s.report();
        if (s.failed())
            break;

        std::size_t rest_data_i_end = rest_data.size();
        const std::size_t nr_syms_decoded = rest_data_i_end - rest_data_i;

        EXPECT_EQ(nr_syms_encoded, nr_syms_decoded);

        if (HasFailure())
            print_to(std::cerr, token);
    }

    if (HasFailure())
        print_to(std::cerr, '\n');

    ASSERT_THAT(rest_data, Pointwise(Eq(), data));
}

TEST_P(LZ77Test, EncodeDecodePackedTokens)
{
    std::vector<char> data {std::get<0>(GetParam())->get_data()};
    auto encoder = DeflateLZ77::make_encoder(std::get<1>(GetParam()), data.begin(), data.end());
    std::vector<char> rest_data;
    auto decoder = DeflateLZ77::make_decoder(std::back_inserter(rest_data));

    std::vector<DeflateLZ77::PackedToken> buffer;
    ASSERT_TRUE(encoder.encode(std::back_inserter(buffer)).is_none());

    for (std::size_t i = 0; i < buffer.size(); ++i) {
        DeflateLZ77::PackedToken p_token = buffer[i];
        if (p_token.is_len_dist()) {
            ASSERT_LT(++i, buffer.size());
            const DeflateLZ77::DistExtra dist_extra = buffer[i].get_dist_extra();
            auto s = decoder.decode_once(p_token.get_len_sym(), p_token.get_len_extra(),
                                         p_token.get_dist_sym(), dist_extra);
            EXPECT_TRUE(s.successful()) << s.report();
            if (s.failed())
                break;
        } else {
            decoder.decode_once(p_token.get_literal());
        }
    }

    ASSERT_THAT(rest_data, Pointwise(Eq(), data));
}

static std::vector<TestOnDataInputCustom> gen_custom_inputs()
{
    std::vector<TestOnDataInputCustom> custom_inputs;
    custom_inputs = get_default_custom_inputs();
    custom_inputs.emplace_back(std::vector<char>(1 << 18, '0'));
    return custom_inputs;
};

static const auto test_inputs = make_generated_test_on_data_inputs(16, 1234, 16, gen_custom_inputs());

#define SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_LZ77_TEST(level) \
    INSTANTIATE_TEST_SUITE_P(Level_##level, LZ77Test, \
            ::testing::Combine(::testing::ValuesIn(test_inputs.collect()), \
                               ::testing::Values(get_lz77_encoder_params_for(level))))

SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_LZ77_TEST(0);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_LZ77_TEST(1);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_LZ77_TEST(2);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_LZ77_TEST(3);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_LZ77_TEST(4);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_LZ77_TEST(5);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_LZ77_TEST(6);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_LZ77_TEST(7);
SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_LZ77_TEST(8);

#undef SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_LZ77_TEST

}
