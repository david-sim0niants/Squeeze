#include <gtest/gtest.h>

#include "squeeze/compression/huffman.h"
#include "squeeze/compression/deflate_huffman.h"
#include "squeeze/compression/huffman_15.h"
#include "squeeze/utils/overloaded.h"

#include "../test_tools/random.h"
#include "../test_tools/generate_mock_contents.h"

namespace squeeze::compression::testing {

class HuffmanFreqTestInput {
public:
    virtual std::vector<unsigned int> get_freqs() const = 0;
};

class HuffmanFreqTestInputRandom : public HuffmanFreqTestInput {
public:
    HuffmanFreqTestInputRandom(
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

private:
    int seed;
    int min_freq, max_freq;
    std::size_t min_nr_freqs, max_nr_freqs;
};

class HuffmanFreqTestInputCustom : public HuffmanFreqTestInput {
public:
    HuffmanFreqTestInputCustom(std::vector<unsigned int>&& freqs) : freqs(std::move(freqs))
    {
    }

    std::vector<unsigned int> get_freqs() const override
    {
        return freqs;
    }

private:
    std::vector<unsigned int> freqs;
};

void PrintTo(const HuffmanFreqTestInput *value, std::ostream *os)
{
    *os << "Fs: { ";
    for (auto freq : value->get_freqs())
        *os << freq << ' ';
    *os << '}' << std::endl;
}

class HuffmanFreqBasedTest : public ::testing::TestWithParam<const HuffmanFreqTestInput *> {
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

TEST_P(HuffmanFreqBasedTest, GenCodeLens)
{
    std::vector<unsigned int> code_lens = gen_code_lengths(GetParam()->get_freqs());
    EXPECT_TRUE(compression::Huffman<>::validate_code_lens(code_lens.begin(), code_lens.end()));
}

TEST_P(HuffmanFreqBasedTest, GenCodes)
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

TEST_P(HuffmanFreqBasedTest, MakeTree)
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
        ASSERT_NE(tree.get_root(), nullptr);
        ASSERT_NE(tree.get_root()->get_left(), nullptr);
        ASSERT_NE(tree.get_root()->get_right(), nullptr);
        EXPECT_TRUE(tree.get_root()->get_left()->is_leaf());
        EXPECT_TRUE(tree.get_root()->get_right()->is_leaf());
        EXPECT_EQ(tree.get_root()->get_left()->get_symbol(), 0);
        EXPECT_EQ(tree.get_root()->get_right()->get_symbol(), HuffmanTreeNode::sentinel_symbol);
    default:
        ASSERT_NE(tree.get_root(), nullptr);
        EXPECT_TRUE(tree.get_root()->validate_full_tree());
        break;
    }
}

TEST_P(HuffmanFreqBasedTest, EncodeDecodeCodeLenCodeLens)
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

    EXPECT_EQ(clcl_size, rest_clcl_size);
    for (std::size_t i = 0; i < DeflateHuffman<>::code_len_alphabet_size; ++i)
        EXPECT_EQ(clcl[i], rest_clcl[i]);
}

TEST_P(HuffmanFreqBasedTest, EncodeDecodeCodeLens)
{
    std::vector<unsigned int> code_lens = gen_code_lengths(GetParam()->get_freqs());
    ASSERT_TRUE(compression::Huffman<>::validate_code_lens(code_lens.begin(), code_lens.end()));

    DeflateHuffman<>::CodeLenCodeLen clcl[DeflateHuffman<>::code_len_alphabet_size] {};
    DeflateHuffman<>::gen_code_len_code_lens(code_lens.begin(), code_lens.end(), clcl);

    ASSERT_TRUE(DeflateHuffman<>::validate_code_len_code_lens(std::begin(clcl), std::end(clcl)));

    std::size_t clcl_size = std::size(clcl);
    for (; clcl_size > DeflateHuffman<>::min_nr_code_len_codes && clcl[clcl_size - 1] == 0; --clcl_size);

    DeflateHuffman<>::CodeLenCode clc[std::size(clcl)];
    DeflateHuffman<>::gen_code_len_codes(clcl, clcl + clcl_size, clc);

    std::vector<char> buffer;
    auto bit_encoder = misc::make_bit_encoder(std::back_inserter(buffer));
    auto dh_encoder  = DeflateHuffman<>::make_encoder(bit_encoder);
    EXPECT_EQ(dh_encoder.encode_code_len_syms(clc, clcl, code_lens.begin(), code_lens.end()), 0);
    EXPECT_EQ(bit_encoder.finalize(), 0);

    std::vector<unsigned int> rest_code_lens(code_lens.size());

    HuffmanTree tree;
    ASSERT_TRUE(tree.build_from_codes(clc, clc + clcl_size, clcl, clcl + clcl_size).successful());
    const HuffmanTreeNode *root = tree.get_root();
    ASSERT_TRUE(root == nullptr || root->validate_full_tree());

    auto bit_decoder = misc::make_bit_decoder(std::begin(buffer));
    auto dh_decoder = DeflateHuffman<>::make_decoder(bit_decoder);

    EXPECT_EQ(dh_decoder.decode_code_len_syms(root, rest_code_lens.begin(), rest_code_lens.end()), 0);

    for (std::size_t i = 0; i < code_lens.size(); ++i)
        EXPECT_EQ(code_lens[i], rest_code_lens[i]);
}

class GeneratedFreqTestInputs {
public:
    std::vector<const HuffmanFreqTestInput *> collect() const
    {
        std::vector<const HuffmanFreqTestInput *> inputs;
        inputs.reserve(random_inputs.size() + custom_inputs.size());
        for (auto& random_input : random_inputs)
            inputs.emplace_back(&random_input);
        for (auto& custom_input : custom_inputs)
            inputs.emplace_back(&custom_input);
        return inputs;
    }

    static const GeneratedFreqTestInputs instance;

private:
    GeneratedFreqTestInputs()
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
        custom_inputs.push_back(HuffmanFreqTestInputCustom({1, 2, 4, 8, 16, 32}));
        custom_inputs.push_back(HuffmanFreqTestInputCustom({1, 1, 1, 1, 1, 64}));
        custom_inputs.push_back(HuffmanFreqTestInputCustom({1}));
        custom_inputs.push_back(HuffmanFreqTestInputCustom({0, 0, 0}));
        custom_inputs.push_back(HuffmanFreqTestInputCustom({}));
    }

    std::vector<HuffmanFreqTestInputRandom> random_inputs;
    std::vector<HuffmanFreqTestInputCustom> custom_inputs;
};

const GeneratedFreqTestInputs GeneratedFreqTestInputs::instance;

INSTANTIATE_TEST_SUITE_P(Freq, HuffmanFreqBasedTest,
        ::testing::ValuesIn(GeneratedFreqTestInputs::instance.collect()));


class HuffmanDataTestInput {
public:
    virtual std::vector<char> get_data() const = 0;
};

class HuffmanDataTestInputCustom : public HuffmanDataTestInput {
public:
    explicit HuffmanDataTestInputCustom(std::vector<char>&& data) : data(std::move(data))
    {
    }

    std::vector<char> get_data() const override
    {
        return data;
    }

private:
    std::vector<char> data;
};

class HuffmanDataTestInputRandom : public HuffmanDataTestInput {
public:
    explicit HuffmanDataTestInputRandom(int prng_seed, std::string_view content_seed)
        : prng_seed(prng_seed), content_seed(content_seed)
    {
    }

    std::vector<char> get_data() const override
    {
        using namespace ::squeeze::testing::tools;
        const MockContentsGeneratorParams params = {
            .size_limit = 2 << 20,
            .min_nr_reps = 20,
            .max_nr_reps = 2000,
            .noise_probability = 10,
            .flags = MockContentsGeneratorParams::ApplyNoise
                   | MockContentsGeneratorParams::RandomizedRange
                   | MockContentsGeneratorParams::RandomizedRepCount,
        };
        std::stringstream contents;
        generate_mock_contents(params, Random<int>(prng_seed), content_seed, contents);
        return std::vector(contents.view().begin(), contents.view().end());
    }

private:
    int prng_seed;
    std::string_view content_seed;
};

class HuffmanDataBasedTest : public ::testing::TestWithParam<const HuffmanDataTestInput *> {
public:
    static void count_freqs(const std::vector<char>& data, std::array<Huffman<>::CodeLen, 256>& freqs)
    {
        for (char c : data)
            ++freqs[static_cast<unsigned char>(c)];
    }
};

TEST_P(HuffmanDataBasedTest, EncodeDecodeCodes)
{
    std::vector<char> data = GetParam()->get_data();

    std::array<Huffman<>::Freq, 256> freqs {};
    count_freqs(data, freqs);

    std::array<Huffman<>::CodeLen, 256> code_lens {};
    Huffman<>::sort_find_code_lengths(freqs.begin(), freqs.end(), code_lens.begin());
    ASSERT_TRUE(Huffman<>::validate_code_lens(code_lens.begin(), code_lens.end()));

    std::array<Huffman<>::Code, 256> codes {};
    Huffman<>::gen_codes(code_lens.begin(), code_lens.end(), codes.begin());

    std::vector<char> buffer;
    auto bit_encoder = misc::make_bit_encoder(std::back_inserter(buffer));
    auto huffman_encoder = Huffman<>::make_encoder(bit_encoder);

    auto e = huffman_encoder.encode_codes(codes.data(), code_lens.data(), data.begin(), data.end());
    ASSERT_TRUE(e.successful()) << e.report();
    ASSERT_EQ(bit_encoder.finalize(), 0);

    bit_encoder.finalize();

    const double compression_ratio = data.size() / (double)buffer.size();
    std::cerr << "\033[32m" "Compression ratio: " << compression_ratio
        << " | Compressed " << 100 * (1 - 1.0 / compression_ratio) << '%' << "\033[0m\n";
    EXPECT_GE(compression_ratio, 1.0F);

    auto bit_decoder = misc::make_bit_decoder(buffer.begin());
    auto huffman_decoder = Huffman<>::make_decoder(bit_decoder);
    HuffmanTree tree;
    ASSERT_FALSE(tree.build_from_codes(codes.begin(), codes.end(), code_lens.begin(), code_lens.end()));
    ASSERT_TRUE(tree.get_root()->validate_full_tree());

    std::vector<char> rest_data (data.size());
    auto ee = huffman_decoder.decode_codes(tree.get_root(), rest_data.begin(), rest_data.end());
    ASSERT_TRUE(ee.successful()) << ee.report();

    for (std::size_t i = 0; i < data.size(); ++i)
        ASSERT_EQ(data[i], rest_data[i]);
}

TEST_P(HuffmanDataBasedTest, EncodeDecodeData)
{
    std::vector<char> data = GetParam()->get_data();
    std::vector<char> buffer;
    auto bit_encoder = misc::make_bit_encoder(std::back_inserter(buffer));
    auto huffman15_encoder = Huffman15<>::make_encoder(bit_encoder);

    auto ene = huffman15_encoder.encode_data(data.begin(), data.end());
    ASSERT_TRUE(ene.successful()) << ene.report();
    ASSERT_EQ(bit_encoder.finalize(), 0);

    const double compression_ratio = data.size() / (double)buffer.size();
    std::cerr << "\033[32m" "Compression ratio: " << compression_ratio
        << " | Compressed " << 100 * (1 - 1.0 / compression_ratio) << '%' << "\033[0m\n";
    EXPECT_GE(compression_ratio, 1.0F);

    auto bit_decoder = misc::make_bit_decoder(buffer.begin(), buffer.end());
    auto huffman15_decoder = Huffman15<>::make_decoder(bit_decoder);

    std::vector<char> rest_data(data.size());
    auto dee = huffman15_decoder.decode_data(rest_data.begin(), rest_data.end());
    EXPECT_TRUE(dee.successful()) << dee.report();

    for (std::size_t i = 0; i < data.size(); ++i)
        ASSERT_EQ(data[i], rest_data[i]);
}

TEST_P(HuffmanDataBasedTest, EncodeDecode)
{
    std::vector<char> data = GetParam()->get_data();
    std::vector<char> buffer;
    auto ene = huffman15_encode(data.begin(), data.end(), std::back_inserter(buffer));
    ASSERT_TRUE(ene.successful()) << ene.report();

    const double compression_ratio = data.size() / (double)buffer.size();
    std::cerr << "\033[32m" "Compression ratio: " << compression_ratio
        << " | Compressed " << 100 * (1 - 1.0 / compression_ratio) << '%' << "\033[0m\n";
    EXPECT_GE(compression_ratio, 1.0F);

    std::vector<char> rest_data(data.size());
    auto dee = huffman15_decode(rest_data.begin(), rest_data.end(), buffer.begin(), buffer.end());
    EXPECT_TRUE(dee.successful()) << dee.report();

    for (std::size_t i = 0; i < data.size(); ++i)
        ASSERT_EQ(data[i], rest_data[i]);
}

class GeneratedDataTestInputs {
public:
    std::vector<const HuffmanDataTestInput *> collect() const
    {
        std::vector<const HuffmanDataTestInput *> inputs;
        inputs.reserve(random_inputs.size() + custom_inputs.size());
        for (auto& input : random_inputs)
            inputs.push_back(&input);
        for (auto& input : custom_inputs)
            inputs.push_back(&input);
        return inputs;
    }

    static const GeneratedDataTestInputs instance;

private:
    GeneratedDataTestInputs()
    {
        init_random_inputs();
        init_custom_inputs();
    }

    void init_random_inputs()
    {
        constexpr std::size_t nr_random_test_inputs = 128;
        random_inputs.reserve(nr_random_test_inputs);
        for (std::size_t i = 0; i < nr_random_test_inputs; ++i)
            random_inputs.emplace_back(i + 1234, content_seed);
    }

    void init_custom_inputs()
    {
    }

    static constexpr char content_seed[] = "Pellentesque finibus feugiat quam, non posuere velit varius sed. Pellentesque in consequat mauris, in elementum sapien. In pellentesque consectetur nisl, vel aliquam lectus accumsan ut. Morbi sed efficitur tellus. Sed tincidunt, eros ut tincidunt sagittis, tortor metus rutrum justo, eget sollicitudin libero leo rhoncus est. Sed efficitur nisi id mauris dictum, sit amet hendrerit risus efficitur. Integer ac pharetra lacus. Maecenas eget risus lacinia, volutpat libero at, dignissim nibh. Sed viverra libero et ipsum tempor, quis sagittis neque hendrerit. Cras nibh tortor, blandit in pretium quis, ornare non mauris. In vitae fermentum tortor, quis euismod sem.";

    std::vector<HuffmanDataTestInputRandom> random_inputs;
    std::vector<HuffmanDataTestInputCustom> custom_inputs;
};

const GeneratedDataTestInputs GeneratedDataTestInputs::instance;

INSTANTIATE_TEST_SUITE_P(Data, HuffmanDataBasedTest,
        ::testing::ValuesIn(GeneratedDataTestInputs::instance.collect()));
}
