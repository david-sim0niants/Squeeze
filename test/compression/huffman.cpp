#include <gtest/gtest.h>

#include "squeeze/compression/huffman.h"
#include "../test_tools/random.h"
#include "squeeze/utils/overloaded.h"

namespace squeeze::compression::testing {

struct HuffmanTestInputRandom {
    HuffmanTestInputRandom(
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

struct HuffmanTestInputCustom {
    HuffmanTestInputCustom(std::vector<unsigned int>&& freqs) : freqs(std::move(freqs))
    {
    }

    std::vector<unsigned int> get_freqs() const
    {
        return freqs;
    }

    std::vector<unsigned int> freqs;
};

using HuffmanTestInput = std::variant<HuffmanTestInputRandom, HuffmanTestInputCustom>;

static auto get_freqs(const HuffmanTestInput& huffman_test_input)
{
    return std::visit(utils::Overloaded {
            [](const HuffmanTestInputRandom& input) { return input.get_freqs(); },
            [](const HuffmanTestInputCustom& input) { return input.get_freqs(); }
        }, huffman_test_input);
}

class HuffmanTest : public ::testing::TestWithParam<HuffmanTestInput> {
protected:
    static std::vector<unsigned int> gen_code_lengths(const std::vector<unsigned int>& freqs)
    {
        std::vector<unsigned int> code_lens (freqs.size());
        compression::Huffman<>::sort_find_code_lengths(freqs.begin(), freqs.end(), code_lens.begin());
        return code_lens;
    }

    static std::vector<Huffman<>::Code> gen_codes(const std::vector<unsigned int>& code_lens)
    {
        std::vector<Huffman<>::Code> codes (code_lens.size());
        Huffman<>::gen_codes(code_lens.begin(), code_lens.end(), codes.begin());
        return codes;
    }
};

TEST_P(HuffmanTest, GenCodeLens)
{
    std::vector<unsigned int> code_lens = gen_code_lengths(get_freqs(GetParam()));
    EXPECT_TRUE(compression::Huffman<>::validate_code_lens(code_lens.begin(), code_lens.end()));
}

TEST_P(HuffmanTest, GenCodes)
{
    std::vector<unsigned int> code_lens = gen_code_lengths(get_freqs(GetParam()));
    ASSERT_TRUE(compression::Huffman<>::validate_code_lens(code_lens.begin(), code_lens.end()));
    std::vector<Huffman<>::Code> codes = gen_codes(code_lens);
    ASSERT_EQ(codes.size(), code_lens.size());
    for (std::size_t i = 0; i < codes.size(); ++i) {
        ASSERT_GT(code_lens[i], 0);
        ASSERT_LE(code_lens[i], codes[i].size());
        EXPECT_TRUE(
            (codes[i] & Huffman<>::Code(((unsigned long long)(1) << (code_lens[i])) - 1)) == codes[i]);
    }
}

TEST_P(HuffmanTest, MakeTree)
{
    std::vector<unsigned int> code_lens = gen_code_lengths(get_freqs(GetParam()));
    ASSERT_TRUE(compression::Huffman<>::validate_code_lens(code_lens.begin(), code_lens.end()));
    std::vector<Huffman<>::Code> codes = gen_codes(code_lens);
    ASSERT_EQ(codes.size(), code_lens.size());
    HuffmanTree tree;
    EXPECT_TRUE(Huffman<>::make_tree(codes.begin(), codes.end(),
                code_lens.begin(), code_lens.end(), tree).successful());
    EXPECT_TRUE(tree.get_root()->validate_full_tree());
}

namespace {

auto make_huffman_test_inputs()
{
    constexpr std::size_t nr_test_inputs = 128;
    std::vector<HuffmanTestInput> huffman_test_inputs;
    huffman_test_inputs.reserve(nr_test_inputs);
    for (std::size_t i = 0; i < huffman_test_inputs.capacity() - 2; ++i)
        huffman_test_inputs.push_back(
                HuffmanTestInputRandom(i + 1234, 1, nr_test_inputs, i + 1, i + nr_test_inputs));
    huffman_test_inputs.push_back(HuffmanTestInputCustom({1, 2, 4, 8, 16, 32}));
    huffman_test_inputs.push_back(HuffmanTestInputCustom({1, 1, 1, 1, 1, 64}));
    return huffman_test_inputs;
}

}

INSTANTIATE_TEST_SUITE_P(AnyFrequencies, HuffmanTest, ::testing::ValuesIn(make_huffman_test_inputs()));

}
