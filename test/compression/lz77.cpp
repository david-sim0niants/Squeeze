#include <gtest/gtest.h>

#include "squeeze/compression/lz77.h"
#include "squeeze/utils/overloaded.h"

#include "../test/test_tools/generate_mock_contents.h"

namespace squeeze::compression::testing {

class LZ77TestInput {
public:
    LZ77TestInput(int prng_seed, std::string_view content_seed)
        : prng_seed(prng_seed), content_seed(content_seed)
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

private:
    int prng_seed;
    std::string_view content_seed;
};

class LZ77Test : public ::testing::TestWithParam<LZ77TestInput> {};

TEST_P(LZ77Test, EncodeDecodeTokens)
{
    std::vector<char> data = GetParam().gen_data();
    auto encoder = LZ77<>::make_encoder(1 << 15, 258, data.begin(), data.end());
    std::vector<char> rest_data;
    auto decoder = LZ77<>::make_decoder(1 << 15, 258, std::back_inserter(rest_data));

    while (true) {
        auto data_it = encoder.get_it();
        LZ77<>::Token token = encoder.encode_once();
        if (std::holds_alternative<std::monostate>(token))
            break;
        const std::size_t nr_literals_encoded = LZ77<>::get_nr_literals_within_token(token);

        std::size_t rest_data_i = rest_data.size();
        auto e = decoder.decode_once(token);
        ASSERT_TRUE(e.successful()) << e.report();
        std::size_t rest_data_i_end = rest_data.size();
        const std::size_t nr_literals_decoded = rest_data_i_end - rest_data_i;

        EXPECT_EQ(nr_literals_encoded, nr_literals_decoded);
    }

    ASSERT_EQ(data.size(), rest_data.size());
    for (std::size_t i = 0; i < data.size(); ++i) {
        ASSERT_EQ(data[i], rest_data[i]);
    }
}

static constexpr char content_seed[] = "Aliquam convallis ornare luctus. Mauris semper enim sit amet leo maximus, sit amet varius massa consectetur. Sed ut bibendum orci, vitae placerat massa. Mauris pulvinar, mi sit amet maximus eleifend, augue orci fringilla eros, ac posuere massa nisi id sem. Nam malesuada eleifend risus, ac aliquet augue finibus ac. Duis et turpis elit. Sed in vehicula dolor, vel commodo urna.";

std::vector<LZ77TestInput> gen_test_inputs()
{
    std::vector<LZ77TestInput> inputs;
    constexpr std::size_t nr_tests = 16;
    for (std::size_t i = 0; i < nr_tests; ++i)
        inputs.emplace_back(1234 + i, content_seed);
    return inputs;
}

INSTANTIATE_TEST_SUITE_P(Default, LZ77Test, ::testing::ValuesIn(gen_test_inputs()));

}
