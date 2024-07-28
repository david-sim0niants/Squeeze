#include <gtest/gtest.h>

#include "squeeze/compression/huffman.h"
#include "../test_tools/random.h"

namespace squeeze::compression::testing {

struct HuffmanTestInput {
    HuffmanTestInput(
            int seed, int min_freq, int max_freq,
            std::size_t min_nr_freqs, std::size_t max_nr_freqs)
        :   seed(seed), min_freq(min_freq), max_freq(max_freq),
            min_nr_freqs(min_nr_freqs), max_nr_freqs(max_nr_freqs)
    {
    }

    std::vector<unsigned int> get_freqs() const
    {
        squeeze::testing::tools::Random<int> random(seed);
        std::vector<unsigned int> freqs(random(min_nr_freqs, max_nr_freqs));
        for (std::size_t i = 0; i < freqs.size(); ++i)
            freqs[i] = random(min_freq, max_freq);
        return freqs;
    }

    int seed;
    int min_freq, max_freq;
    std::size_t min_nr_freqs, max_nr_freqs;
};

class HuffmanTest : public ::testing::TestWithParam<HuffmanTestInput> {};

TEST_P(HuffmanTest, GenCodeLens)
{
    std::vector<unsigned int> freqs = GetParam().get_freqs();
    std::vector<unsigned int> code_lens (freqs.size());
    compression::Huffman<>::sort_find_code_lengths(freqs.begin(), freqs.end(), code_lens.begin());
    EXPECT_TRUE(compression::Huffman<>::validate_code_lens(code_lens.begin(), code_lens.end()));
}

namespace {
auto make_huffman_test_inputs()
{
    constexpr std::size_t nr_test_inputs = 128;
    std::vector<HuffmanTestInput> huffman_test_inputs;
    huffman_test_inputs.reserve(nr_test_inputs);
    for (std::size_t i = 0; i < huffman_test_inputs.capacity(); ++i)
        huffman_test_inputs.emplace_back(i + 1234, 1, nr_test_inputs, i + 1, i + nr_test_inputs);
    return huffman_test_inputs;
}
}

INSTANTIATE_TEST_SUITE_P(AnyFrequencies, HuffmanTest, ::testing::ValuesIn(make_huffman_test_inputs()));

}
