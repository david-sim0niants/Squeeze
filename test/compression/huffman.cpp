#include <gtest/gtest.h>

#include "squeeze/compression/huffman.h"
#include "../test_tools/random.h"
#include "squeeze/utils/overloaded.h"

namespace squeeze::compression::testing {

struct HuffmanTestInput {
    virtual std::vector<unsigned int> get_freqs() const = 0;
};

struct HuffmanTestInputRandom : HuffmanTestInput {
    HuffmanTestInputRandom(
            int seed, int min_freq, int max_freq,
            std::size_t min_nr_freqs, std::size_t max_nr_freqs)
        :   seed(seed), min_freq(min_freq), max_freq(max_freq),
            min_nr_freqs(min_nr_freqs), max_nr_freqs(max_nr_freqs)
    {
    }

    std::vector<unsigned int> get_freqs() const override
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

struct HuffmanTestInputCustom : HuffmanTestInput {
    HuffmanTestInputCustom(std::vector<unsigned int>&& freqs) : freqs(std::move(freqs))
    {
    }

    std::vector<unsigned int> get_freqs() const override
    {
        return freqs;
    }

    std::vector<unsigned int> freqs;
};

void PrintTo(const HuffmanTestInput *value, std::ostream *os)
{
    *os << "Fs: { ";
    for (auto freq : value->get_freqs())
        *os << freq << ' ';
    *os << '}' << std::endl;
}

class HuffmanTest : public ::testing::TestWithParam<const HuffmanTestInput *> {
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
    std::vector<unsigned int> code_lens = gen_code_lengths(GetParam()->get_freqs());
    EXPECT_TRUE(compression::Huffman<>::validate_code_lens(code_lens.begin(), code_lens.end()));
}

TEST_P(HuffmanTest, GenCodes)
{
    std::vector<unsigned int> code_lens = gen_code_lengths(GetParam()->get_freqs());
    ASSERT_TRUE(compression::Huffman<>::validate_code_lens(code_lens.begin(), code_lens.end()));
    std::vector<Huffman<>::Code> codes = gen_codes(code_lens);
    ASSERT_EQ(codes.size(), code_lens.size());
    for (std::size_t i = 0; i < codes.size(); ++i) {
        if (code_lens[i] == 0)
            continue;
        ASSERT_LE(code_lens[i], codes[i].size());
        EXPECT_TRUE((codes[i] & Huffman<>::Code((1ULL << code_lens[i]) - 1)) == codes[i]);
    }
}

TEST_P(HuffmanTest, MakeTree)
{
    std::vector<unsigned int> code_lens = gen_code_lengths(GetParam()->get_freqs());
    ASSERT_TRUE(compression::Huffman<>::validate_code_lens(code_lens.begin(), code_lens.end()));
    std::vector<Huffman<>::Code> codes = gen_codes(code_lens);
    ASSERT_EQ(codes.size(), code_lens.size());
    HuffmanTree tree;
    EXPECT_TRUE(tree.build_from_codes(codes.begin(), codes.end(),
                    code_lens.begin(), code_lens.end()).successful());

    std::size_t nr_nz_code_lens = 0;
    for (auto code_len : code_lens)
        nr_nz_code_lens += code_len != 0;

    switch (nr_nz_code_lens) {
    case 0:
        EXPECT_TRUE(tree.get_root() == nullptr);
        break;
    case 1:
        EXPECT_TRUE(tree.get_root()->get_left() != nullptr &&
                tree.get_root()->get_left()->is_leaf() && tree.get_root()->get_right() == nullptr);
        break;
    default:
        EXPECT_TRUE(tree.get_root() != nullptr && tree.get_root()->validate_full_tree());
        break;
    }
}

TEST_P(HuffmanTest, EncodeDecodeCodeLenCodeLens)
{
    std::vector<unsigned int> code_lens = gen_code_lengths(GetParam()->get_freqs());
    ASSERT_TRUE(compression::Huffman<>::validate_code_lens(code_lens.begin(), code_lens.end()));

    DeflateHuffman<>::CodeLenCodeLen clcl[DeflateHuffman<>::code_len_alphabet_size] {};
    DeflateHuffman<>::gen_code_len_code_lens(code_lens.begin(), code_lens.end(), clcl);

    ASSERT_TRUE(DeflateHuffman<>::validate_code_len_code_lens(std::begin(clcl), std::end(clcl)));

    std::size_t clcl_size = std::size(clcl);
    for (; clcl_size > DeflateHuffman<>::min_nr_code_len_codes && clcl[clcl_size - 1] == 0; --clcl_size);

    constexpr std::size_t buffer_size = (DeflateHuffman<>::code_len_alphabet_size * 3 + 4 + 7) / 8;
    char buffer[buffer_size] {};

    auto bit_encoder = misc::make_bit_encoder(buffer);
    auto dh_encoder = DeflateHuffman<>::make_encoder(bit_encoder);
    EXPECT_EQ(dh_encoder.encode_nr_code_len_codes(clcl_size), 0);
    EXPECT_EQ(dh_encoder.encode_code_len_code_lens(clcl, clcl + clcl_size), 0);
    EXPECT_EQ(bit_encoder.finalize(), 0);

    DeflateHuffman<>::CodeLenCodeLen rest_clcl[DeflateHuffman<>::code_len_alphabet_size] {};
    std::size_t rest_clcl_size = 0;

    auto bit_decoder = misc::make_bit_decoder(buffer);
    auto dh_decoder = DeflateHuffman<>::make_decoder(bit_decoder);
    EXPECT_EQ(dh_decoder.decode_nr_code_len_codes(rest_clcl_size), 0);
    EXPECT_EQ(dh_decoder.decode_code_len_code_lens(rest_clcl, rest_clcl + rest_clcl_size), 0);

    for (std::size_t i = 0; i < DeflateHuffman<>::code_len_alphabet_size; ++i)
        EXPECT_EQ(clcl[i], rest_clcl[i]);
}

class GeneratedTestInputs {
public:
    std::vector<const HuffmanTestInput *> collect() const
    {
        std::vector<const HuffmanTestInput *> inputs;
        inputs.reserve(random_inputs.size() + custom_inputs.size());
        for (auto& random_input : random_inputs)
            inputs.emplace_back(&random_input);
        for (auto& custom_input : custom_inputs)
            inputs.emplace_back(&custom_input);
        return inputs;
    }

    static const GeneratedTestInputs instance;

private:
    GeneratedTestInputs()
    {
        init_random_inputs();
        init_custom_inputs();
    }

    void init_random_inputs()
    {
        constexpr std::size_t nr_random_test_inputs = 123;
        random_inputs.reserve(nr_random_test_inputs);
        for (std::size_t i = 0; i < nr_random_test_inputs; ++i)
            random_inputs.emplace_back(i + 1234, 0, nr_random_test_inputs,
                    i + 1, i + nr_random_test_inputs);
    }

    void init_custom_inputs()
    {
        constexpr std::size_t nr_custom_test_inputs = 5;
        custom_inputs.reserve(nr_custom_test_inputs);
        custom_inputs.push_back(HuffmanTestInputCustom({1, 2, 4, 8, 16, 32}));
        custom_inputs.push_back(HuffmanTestInputCustom({1, 1, 1, 1, 1, 64}));
        custom_inputs.push_back(HuffmanTestInputCustom({1}));
        custom_inputs.push_back(HuffmanTestInputCustom({0, 0, 0}));
        custom_inputs.push_back(HuffmanTestInputCustom({}));
    }

    std::vector<HuffmanTestInputRandom> random_inputs;
    std::vector<HuffmanTestInputCustom> custom_inputs;
};

const GeneratedTestInputs GeneratedTestInputs::instance;

INSTANTIATE_TEST_SUITE_P(AnyFrequencies, HuffmanTest,
        ::testing::ValuesIn(GeneratedTestInputs::instance.collect()));

}
