// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "squeeze/compression/huffman.h"

#include "test_tools/generators/prng.h"
#include "test_tools/generators/test_gen.h"
#include "test_common/test_on_data_input.h"

namespace squeeze::compression::testing {

using ::testing::WithParamInterface;
using ::testing::Eq;
using ::testing::Pointwise;
using ::testing::Truly;

using test_tools::generators::make_generated_test_inputs;
using test_tools::generators::PRNG;

using namespace test_common;

class TestOnFreqInput {
public:
    virtual std::vector<Huffman<>::Freq> get_freqs() const = 0;
};

class HuffmanTest : public ::testing::Test {
protected:
    static std::vector<Huffman<>::CodeLen> gen_code_lengths(const std::vector<Huffman<>::Freq>& freqs)
    {
        std::vector<Huffman<>::CodeLen> code_lens (freqs.size());
        compression::Huffman<>::find_code_lengths(freqs.begin(), freqs.size(), code_lens.begin());
        return code_lens;
    }

    static std::vector<Huffman<>::Code> gen_codes(const std::vector<Huffman<>::CodeLen>& code_lens)
    {
        std::vector<Huffman<>::Code> codes (code_lens.size());
        Huffman<>::gen_codes(code_lens.begin(), code_lens.end(), codes.begin());
        return codes;
    }

    template<std::input_iterator InIt>
    inline static std::size_t count_non_zero_lens(InIt code_len_it, InIt code_len_it_end)
    {
        std::size_t nr_nz_code_lens = 0;
        for (; code_len_it != code_len_it_end; ++code_len_it)
            nr_nz_code_lens += *code_len_it != 0;
        return nr_nz_code_lens;
    }

    inline static std::size_t count_leaves(const HuffmanTreeNode *node)
    {
        return node ? node->is_leaf() + count_leaves(node->get_left()) + count_leaves(node->get_right()) : 0;
    }

    inline static std::size_t measure_depth(const HuffmanTreeNode *node)
    {
        return node ? !node->is_leaf() + std::max(measure_depth(node->get_left()), measure_depth(node->get_right())) : 0;
    }

    static void count_freqs(const std::vector<char>& data, std::array<Huffman<>::CodeLen, 256>& freqs)
    {
        for (char c : data)
            ++freqs[static_cast<unsigned char>(c)];
    }

    static void gen_codes_and_code_lens(const std::vector<char>& data,
            std::array<Huffman<>::Code, 256>& codes,
            std::array<Huffman<>::CodeLen, 256>& code_lens)
    {
        std::array<Huffman<>::Freq, 256> freqs {};
        count_freqs(data, freqs);
        Huffman<>::find_code_lengths(freqs.begin(), freqs.size(), code_lens.begin());
        Huffman<>::gen_codes(code_lens.begin(), code_lens.end(), codes.begin());
    }
};

class HuffmanTestOnFreq : public HuffmanTest, public WithParamInterface<const TestOnFreqInput *> {
};

TEST_P(HuffmanTestOnFreq, GeneratesValidCodeLens)
{
    std::vector<Huffman<>::CodeLen> code_lens = gen_code_lengths(GetParam()->get_freqs());
    EXPECT_TRUE(compression::Huffman<>::validate_code_lens(code_lens.begin(), code_lens.end()));
}

TEST_P(HuffmanTestOnFreq, GeneratesValidCodes)
{
    std::vector<Huffman<>::CodeLen> code_lens = gen_code_lengths(GetParam()->get_freqs());
    std::vector<Huffman<>::Code> codes = gen_codes(code_lens);

    auto match = [](std::tuple<Huffman<>::Code, Huffman<>::CodeLen> code_and_code_len)
        {
            auto [code, code_len] = code_and_code_len;
            return (code & Huffman<>::Code((1ULL << code_len) - 1)) == code;
        };

    ASSERT_THAT(codes, Pointwise(Truly(match), code_lens));
}

TEST_P(HuffmanTestOnFreq, ValidCodesMakeValidTree)
{
    std::vector<unsigned int> code_lens = gen_code_lengths(GetParam()->get_freqs());
    std::vector<Huffman<>::Code> codes = gen_codes(code_lens);
    HuffmanTree tree;
    EXPECT_TRUE(tree.build_from_codes(codes.begin(), codes.end(),
                    code_lens.begin(), code_lens.end()).successful());

    const std::size_t nr_non_zero_lens = count_non_zero_lens(code_lens.begin(), code_lens.end());
    const std::size_t nr_leaves = count_leaves(tree.get_root());
    const std::size_t depth = measure_depth(tree.get_root());

    // assert that the number of leaves equals the number of non-zero code lengths,
    // except when the latter is 1, the number of leaves must be 2
    ASSERT_EQ(nr_leaves, nr_non_zero_lens + (nr_non_zero_lens == 1));
    ASSERT_EQ(depth, code_lens.empty() ? 0U : *std::max_element(code_lens.begin(), code_lens.end()));

    switch (nr_non_zero_lens) {
    case 0:
        // assert that if there are no non-zero code lengths, the root is null
        ASSERT_EQ(tree.get_root(), nullptr);
        break;
    case 1:
        // assert that if there's only one non-zero code length, the tree should
        // still be a valid full binary tree with three nodes,
        // where the left child of the root corresponds to the single non-zero-length code,
        // and the right child is the sentinel symbol node
        ASSERT_TRUE(tree.get_root() != nullptr && tree.get_root()->get_right() != nullptr);
        EXPECT_TRUE(tree.get_root()->get_right()->is_leaf());
        EXPECT_EQ(tree.get_root()->get_right()->get_symbol(), HuffmanTreeNode::sentinel_symbol);
        ASSERT_NE(tree.get_root()->get_left(), nullptr);
        ASSERT_TRUE(tree.get_root()->get_left()->is_leaf());
    default:
        // assert that for non-zero code lengths, the root is not null and the tree is a full binary tree
        ASSERT_NE(tree.get_root(), nullptr);
        EXPECT_TRUE(tree.get_root()->validate_full_tree());
        break;
    }
}

TEST_F(HuffmanTestOnFreq, InvalidCodesMakeInvalidTree)
{
    // make invalid codes
    std::vector<unsigned int> code_lens = {1, 2, 2, 2};
    std::vector<Huffman<>::Code> codes = {0b1, 0b10, 0b01, 0b11};
    HuffmanTree tree;
    // assert that the tree building fails
    ASSERT_TRUE(tree.build_from_codes(codes.begin(), codes.end(),
                code_lens.begin(), code_lens.end()).failed());
}

class TestOnFreqInputRandom : public TestOnFreqInput {
public:
    TestOnFreqInputRandom(
            int prng_seed, int min_freq, int max_freq,
            std::size_t min_nr_freqs, std::size_t max_nr_freqs)
        :   prng_seed(prng_seed), min_freq(min_freq), max_freq(max_freq),
            min_nr_freqs(min_nr_freqs), max_nr_freqs(max_nr_freqs)
    {
    }

    std::vector<Huffman<>::Freq> get_freqs() const override
    {
        PRNG prng(prng_seed);
        std::vector<Huffman<>::Freq> freqs(prng(min_nr_freqs, max_nr_freqs));
        for (std::size_t i = 0; i < freqs.size(); ++i)
            freqs[i] = prng(min_freq, max_freq);
        return freqs;
    }

private:
    int prng_seed;
    int min_freq, max_freq;
    std::size_t min_nr_freqs, max_nr_freqs;
};

class TestOnFreqInputCustom : public TestOnFreqInput {
public:
    TestOnFreqInputCustom(std::vector<Huffman<>::Freq>&& freqs) : freqs(std::move(freqs))
    {
    }

    std::vector<Huffman<>::Freq> get_freqs() const override
    {
        return freqs;
    }

private:
    std::vector<Huffman<>::Freq> freqs;
};

void PrintTo(const TestOnFreqInput *value, std::ostream *os)
{
    *os << "Freqs: { ";
    for (auto freq : value->get_freqs())
        *os << freq << ' ';
    *os << '}';
}

namespace {

std::vector<TestOnFreqInputRandom> get_freq_random_inputs()
{
    std::vector<TestOnFreqInputRandom> random_inputs;
    constexpr std::size_t nr_random_test_inputs = 123;
    random_inputs.reserve(nr_random_test_inputs);
    for (std::size_t i = 0; i < nr_random_test_inputs; ++i)
        random_inputs.emplace_back(i + 1234, 0, nr_random_test_inputs,
                i + 1, i + nr_random_test_inputs);
    return random_inputs;
}

std::vector<TestOnFreqInputCustom> get_freq_custom_inputs()
{
    std::vector<TestOnFreqInputCustom> custom_inputs;
    constexpr std::size_t nr_custom_test_inputs = 5;
    custom_inputs.reserve(nr_custom_test_inputs);
    custom_inputs.push_back(TestOnFreqInputCustom({1, 2, 4, 8, 16, 32}));
    custom_inputs.push_back(TestOnFreqInputCustom({1, 1, 1, 1, 1, 64}));
    custom_inputs.push_back(TestOnFreqInputCustom({1}));
    custom_inputs.push_back(TestOnFreqInputCustom({0, 0, 0}));
    custom_inputs.push_back(TestOnFreqInputCustom({}));
    return custom_inputs;
}

const auto test_on_freq_inputs =
    make_generated_test_inputs<TestOnFreqInput>(get_freq_random_inputs(), get_freq_custom_inputs());

}

INSTANTIATE_TEST_SUITE_P(Freq, HuffmanTestOnFreq, ::testing::ValuesIn(test_on_freq_inputs.collect()));

class HuffmanTestOnData : public HuffmanTest, public WithParamInterface<const TestOnDataInput *> {};

TEST_P(HuffmanTestOnData, EncodesDecodesSymbols)
{
    std::vector<char> data = GetParam()->get_data();

    std::array<Huffman<>::Code, 256> codes {};
    std::array<Huffman<>::CodeLen, 256> code_lens {};
    gen_codes_and_code_lens(data, codes, code_lens);

    std::vector<char> buffer;
    auto bit_encoder = misc::make_bit_encoder(std::back_inserter(buffer));
    auto huffman_encoder = Huffman<>::make_encoder(codes.data(), code_lens.data(), bit_encoder);

    auto it = huffman_encoder.encode_syms(data.begin(), data.end());
    ASSERT_EQ(it, data.end());
    ASSERT_EQ(bit_encoder.finalize(), 0);

    const double compression_ratio = data.size() / (double)buffer.size();
    EXPECT_GE(compression_ratio, 1.0F);

    auto bit_decoder = misc::make_bit_decoder(buffer.begin());
    HuffmanTree tree;
    ASSERT_TRUE(tree.build_from_codes(codes.begin(), codes.end(),
                                      code_lens.begin(), code_lens.end()).successful());
    auto huffman_decoder = Huffman<>::make_decoder(tree.get_root(), bit_decoder);

    std::vector<char> rest_data (data.size());
    auto rest_it = huffman_decoder.decode_syms(rest_data.begin(), rest_data.end());
    ASSERT_EQ(rest_it, rest_data.end());

    ASSERT_THAT(rest_data, Pointwise(Eq(), data));
}

static const auto test_inputs = make_generated_test_on_data_inputs(128, 42, 32);

INSTANTIATE_TEST_SUITE_P(Data, HuffmanTestOnData, ::testing::ValuesIn(test_inputs.collect()));

}
