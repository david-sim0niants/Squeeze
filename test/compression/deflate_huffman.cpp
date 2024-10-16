// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "squeeze/compression/deflate_huffman.h"
#include "test_tools/generators/prng.h"
#include "test_tools/generators/test_gen.h"

namespace squeeze::compression::testing {

using ::testing::Eq;
using ::testing::Pointwise;
using test_tools::generators::make_generated_test_inputs;

class TestOnLengthInput {
public:
    virtual std::vector<Huffman<>::CodeLen> get_code_lengths() const = 0;
    virtual void gen_code_len_code_lens(DeflateHuffman<>::CodeLenCodeLen *clcl_data) const = 0;
};

void PrintTo(const TestOnLengthInput *value, std::ostream *os)
{
    *os << "Code Lengths: { ";
    for (auto code_len : value->get_code_lengths())
        *os << code_len << ' ';
    *os << "}, Code Lengths for the Code Length Alphabet: { ";
    std::array<DeflateHuffman<>::CodeLenCodeLen, DeflateHuffman<>::code_len_alphabet_size> clcl {};
    std::size_t clcl_size = std::size(clcl);
    for (; clcl_size > DeflateHuffman<>::min_nr_code_len_codes && clcl[clcl_size - 1] == 0; --clcl_size);

    for (std::size_t i = 0; i < clcl_size; ++i)
        *os << clcl[i] << ' ';
    *os << '}';
}

class DeflateHuffmanTest : public ::testing::TestWithParam<const TestOnLengthInput *> {
protected:
    static constexpr auto cl_alphabet_size = DeflateHuffman<>::code_len_alphabet_size;
};

TEST_P(DeflateHuffmanTest, EncodesDecodesCodeLenCodeLens)
{
    std::array<DeflateHuffman<>::CodeLenCodeLen, cl_alphabet_size> clcl {};
    GetParam()->gen_code_len_code_lens(clcl.data());

    std::size_t clcl_size = std::size(clcl);
    for (; clcl_size > DeflateHuffman<>::min_nr_code_len_codes && clcl[clcl_size - 1] == 0; --clcl_size);

    constexpr std::size_t buffer_size = (cl_alphabet_size * 3 + 4 + 7) / 8;
    char buffer[buffer_size] {};

    auto bit_encoder = misc::make_bit_encoder(buffer);
    auto dh_encoder = DeflateHuffman<>::make_encoder(bit_encoder);
    auto clcl_end = clcl.begin() + clcl_size;
    EXPECT_TRUE(dh_encoder.encode_nr_code_len_codes(clcl_size));
    EXPECT_EQ(dh_encoder.encode_code_len_code_lens(clcl.begin(), clcl_end), clcl_end);
    EXPECT_EQ(bit_encoder.finalize(), 0);

    std::array<DeflateHuffman<>::CodeLenCodeLen, cl_alphabet_size> rest_clcl {};
    std::size_t rest_clcl_size = 0;

    auto bit_decoder = misc::make_bit_decoder(buffer);
    auto dh_decoder = DeflateHuffman<>::make_decoder(bit_decoder);
    EXPECT_TRUE(dh_decoder.decode_nr_code_len_codes(rest_clcl_size));
    auto rest_clcl_end = rest_clcl.begin() + rest_clcl_size;
    EXPECT_EQ(dh_decoder.decode_code_len_code_lens(rest_clcl.begin(), rest_clcl_end), rest_clcl_end);

    ASSERT_THAT(rest_clcl, Pointwise(Eq(), clcl));
}

TEST_P(DeflateHuffmanTest, EncodeDecodeCodeLens)
{
    std::vector<unsigned int> code_lens {GetParam()->get_code_lengths()};

    std::array<DeflateHuffman<>::CodeLenCodeLen, cl_alphabet_size> clcl {};
    DeflateHuffman<>::find_code_len_code_lens(code_lens.begin(), code_lens.end(), clcl.begin());

    std::size_t clcl_size = std::size(clcl);
    for (; clcl_size > DeflateHuffman<>::min_nr_code_len_codes && clcl[clcl_size - 1] == 0; --clcl_size);

    std::array<DeflateHuffman<>::CodeLenCode, std::size(clcl)> clc;
    DeflateHuffman<>::gen_code_len_codes(clcl.begin(), clcl.begin() + clcl_size, clc.begin());

    std::vector<char> buffer;
    auto bit_encoder = misc::make_bit_encoder(std::back_inserter(buffer));
    auto dh_encoder  = DeflateHuffman<>::make_encoder(bit_encoder);
    EXPECT_EQ(dh_encoder.encode_code_len_syms(
                clc.begin(), clcl.begin(), code_lens.begin(), code_lens.end()), code_lens.end());
    EXPECT_EQ(bit_encoder.finalize(), 0);

    HuffmanTree tree;
    ASSERT_TRUE(tree.build_from_codes(clc.begin(), clc.begin() + clcl_size,
                                      clcl.begin(), clcl.begin() + clcl_size).successful());
    const HuffmanTreeNode *root = tree.get_root();
    ASSERT_TRUE(root == nullptr || root->validate_full_tree());

    std::vector<unsigned int> rest_code_lens(code_lens.size());

    auto bit_decoder = misc::make_bit_decoder(std::begin(buffer));
    auto dh_decoder = DeflateHuffman<>::make_decoder(bit_decoder);
    EXPECT_EQ(dh_decoder.decode_code_len_syms(root, rest_code_lens.begin(), rest_code_lens.end()),
              rest_code_lens.end());

    ASSERT_THAT(rest_code_lens, Pointwise(Eq(), code_lens));
}

class DeflateHuffmanTestInputRandom : public TestOnLengthInput {
public:
    DeflateHuffmanTestInputRandom(int prng_seed) : prng_seed(prng_seed)
    {
    }

    std::vector<Huffman<>::CodeLen> get_code_lengths() const override
    {
        test_tools::generators::PRNG prng(prng_seed);
        std::vector<Huffman<>::CodeLen> code_lens(prng(0, 320));
        for (Huffman<>::CodeLen& code_len : code_lens)
            code_len = prng.gen<Huffman<>::CodeLen>(0, Huffman<>::code_len_limit);
        return code_lens;
    }

    void gen_code_len_code_lens(DeflateHuffman<>::CodeLenCodeLen *clcl_data) const override
    {
        test_tools::generators::PRNG prng(prng_seed);
        for (std::size_t i = 0; i < DeflateHuffman<>::code_len_alphabet_size; ++i)
            clcl_data[i] = prng(DeflateHuffman<>::CodeLenCodeLen(i < 4),
                                DeflateHuffman<>::CodeLenHuffman::code_len_limit);
    }

private:
    int prng_seed;
};

class DeflateHuffmanTestInputCustom : public TestOnLengthInput {
public:
    DeflateHuffmanTestInputCustom(
            std::vector<Huffman<>::CodeLen> code_lens,
            std::array<DeflateHuffman<>::CodeLenCodeLen, DeflateHuffman<>::code_len_alphabet_size> clcl_arr)
        : code_lens(std::move(code_lens)), clcl_arr(clcl_arr)
    {
    }

    std::vector<Huffman<>::CodeLen> get_code_lengths() const override
    {
        return code_lens;
    }

    void gen_code_len_code_lens(DeflateHuffman<>::CodeLenCodeLen *clcl_data) const override
    {
        std::copy(clcl_arr.begin(), clcl_arr.end(), clcl_data);
    }

private:
    std::vector<Huffman<>::CodeLen> code_lens;
    std::array<DeflateHuffman<>::CodeLenCodeLen, DeflateHuffman<>::code_len_alphabet_size> clcl_arr;
};

namespace {

std::vector<DeflateHuffmanTestInputRandom> get_random_inputs()
{
    std::vector<DeflateHuffmanTestInputRandom> random_inputs;
    constexpr std::size_t nr_random_test_inputs = 123;
    random_inputs.reserve(nr_random_test_inputs);
    for (std::size_t i = 0; i < nr_random_test_inputs; ++i)
        random_inputs.emplace_back(i + 1234);
    return random_inputs;
}

std::vector<DeflateHuffmanTestInputCustom> get_custom_inputs()
{
    std::vector<DeflateHuffmanTestInputCustom> custom_inputs;
    constexpr std::size_t nr_custom_test_inputs = 5;
    custom_inputs.reserve(nr_custom_test_inputs);
    custom_inputs.push_back(DeflateHuffmanTestInputCustom({15, 15, 15, 15, 13, 12, 12, 12, 12}, {1, 1, 1, 1, 1, 1, 1}));
    custom_inputs.push_back(DeflateHuffmanTestInputCustom({0, 0, 0, 1}, {1, 1, 1, 1, 1, 1, 1}));
    custom_inputs.push_back(DeflateHuffmanTestInputCustom({0, 0, 0}, {4, 2, 1, 6}));
    custom_inputs.push_back(DeflateHuffmanTestInputCustom({1}, {1}));
    custom_inputs.push_back(DeflateHuffmanTestInputCustom({}, {}));
    return custom_inputs;
}

const auto test_inputs =
    make_generated_test_inputs<TestOnLengthInput>(get_random_inputs(), get_custom_inputs());

}

INSTANTIATE_TEST_SUITE_P(CodeLengths, DeflateHuffmanTest, ::testing::ValuesIn(test_inputs.collect()));

}
