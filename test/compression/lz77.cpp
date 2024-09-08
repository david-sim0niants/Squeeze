#include <gtest/gtest.h>

#include "squeeze/compression/lz77.h"
#include "squeeze/compression/deflate_lz77.h"
#include "squeeze/compression/config.h"
#include "squeeze/utils/overloaded.h"

#include "../test/test_tools/generate_mock_contents.h"

namespace squeeze::compression::testing {

class LZ77TestInput {
public:
    LZ77TestInput(LZ77EncoderParams encoder_params, int prng_seed, std::string_view content_seed)
        : encoder_params(encoder_params), prng_seed(prng_seed), content_seed(content_seed)
    {
    }

    std::vector<char> gen_data() const
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
        return std::vector(contents.view().begin(), contents.view().end());
    }

    inline const auto& get_encoder_params() const noexcept
    {
        return encoder_params;
    }

private:
    LZ77EncoderParams encoder_params;
    int prng_seed;
    std::string_view content_seed;
};

class LZ77Test : public ::testing::TestWithParam<LZ77TestInput> {
protected:
    static constexpr std::size_t search_size = 1 << 15;
    static constexpr std::size_t lookahead_size = 258;
};

TEST_P(LZ77Test, EncodeDecodeTokens)
{
    std::vector<char> data = GetParam().gen_data();
    auto encoder = LZ77<>::make_encoder(GetParam().get_encoder_params(),
                                        search_size, lookahead_size, data.begin(), data.end());
    std::vector<char> rest_data;
    auto decoder = LZ77<>::make_decoder(search_size, lookahead_size, std::back_inserter(rest_data));

    while (true) {
        auto data_it = encoder.get_it();
        LZ77<>::Token token = encoder.encode_once();
        if (token.get_type() == LZ77<>::Token::None)
            break;
        const std::size_t nr_syms_encoded = token.get_nr_syms_within();

        std::size_t rest_data_i = rest_data.size();
        auto e = decoder.decode_once(token);
        EXPECT_TRUE(e.successful()) << e.report();
        std::size_t rest_data_i_end = rest_data.size();
        const std::size_t nr_syms_decoded = rest_data_i_end - rest_data_i;

        EXPECT_EQ(nr_syms_encoded, nr_syms_decoded);
    }

    ASSERT_EQ(data.size(), rest_data.size());
    for (std::size_t i = 0; i < data.size(); ++i)
        ASSERT_EQ(data[i], rest_data[i]);
}

TEST_P(LZ77Test, EncodeDecodePackedTokens)
{
    std::vector<char> data = GetParam().gen_data();
    auto encoder = DeflateLZ77::make_encoder(GetParam().get_encoder_params(), data.begin(), data.end());
    std::vector<char> rest_data;
    auto decoder = DeflateLZ77::make_decoder(std::back_inserter(rest_data));

    std::vector<DeflateLZ77::PackedToken> buffer;
    ASSERT_TRUE(encoder.encode(std::back_inserter(buffer)).is_none());
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        DeflateLZ77::PackedToken p_token = buffer[i];
        if (p_token.is_len_dist()) {
            ASSERT_LT(++i, buffer.size());
            DeflateLZ77::DistExtra dist_extra = buffer[i].get_dist_extra();
            decoder.decode_once(p_token.get_len_sym(), p_token.get_len_extra(),
                                p_token.get_dist_sym(), dist_extra);
        } else {
            decoder.decode_once(p_token.get_literal());
        }
    }

    ASSERT_EQ(data.size(), rest_data.size());
    for (std::size_t i = 0; i < data.size(); ++i)
        ASSERT_EQ(data[i], rest_data[i]);
}

static constexpr char content_seed[] = "Aliquam convallis ornare luctus. Mauris semper enim sit amet leo maximus, sit amet varius massa consectetur. Sed ut bibendum orci, vitae placerat massa. Mauris pulvinar, mi sit amet maximus eleifend, augue orci fringilla eros, ac posuere massa nisi id sem. Nam malesuada eleifend risus, ac aliquet augue finibus ac. Duis et turpis elit. Sed in vehicula dolor, vel commodo urna.";

std::vector<LZ77TestInput> gen_test_inputs(std::size_t level)
{
    if (level >= lz77_nr_levels)
        throw BaseException("too high level for LZ77");

    LZ77EncoderParams encoder_params = {
        .lazy_match_threshold = lz77_lazy_match_threshold_per_level[level],
        .match_insert_threshold = lz77_match_insert_threshold_per_level[level],
    };

    std::vector<LZ77TestInput> inputs;
    constexpr std::size_t nr_tests = 16;
    for (std::size_t i = 0; i < nr_tests; ++i)
        inputs.emplace_back(encoder_params, 1234 + i, content_seed);
    return inputs;
}

#define SQUEEZE_COMPRESSION_TESTING_INSTANTIATE_LZ77_TEST(level) \
    INSTANTIATE_TEST_SUITE_P(Level_##level, LZ77Test, ::testing::ValuesIn(gen_test_inputs(level)))

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
