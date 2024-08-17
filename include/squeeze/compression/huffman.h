#pragma once

#include <bitset>
#include <vector>
#include <algorithm>
#include <cassert>

#include "huffman_policy.h"
#include "huffman_tree.h"
#include "squeeze/exception.h"
#include "squeeze/utils/iterator_concepts.h"
#include "squeeze/misc/bitcoder.h"

namespace squeeze::compression {

using squeeze::utils::RandomAccessInputIterator;
using squeeze::utils::RandomAccessOutputIterator;

template<HuffmanPolicy Policy = BasicHuffmanPolicy<15>>
class Huffman {
public:
    using Freq = typename Policy::Freq;
    using CodeLen = typename Policy::CodeLen;
    static constexpr CodeLen code_len_limit = Policy::code_len_limit;
    using Code = std::bitset<code_len_limit>;

private:
    using Symset = std::vector<unsigned int>;

    struct Pack {
        Freq freq;
        Symset symset;

        explicit Pack(Freq freq, Symset&& symset = {}) : freq(freq), symset(std::move(symset))
        {
        }

        explicit Pack(Pack left, Pack right) : freq(left.freq + right.freq), symset()
        {
            symset.reserve(left.symset.size() + right.symset.size());
            symset.insert(symset.end(), left.symset.begin(), left.symset.end());
            symset.insert(symset.end(), right.symset.begin(), right.symset.end());
        }
    };

    using PackSet = std::vector<Pack>;

    enum SortAssume {
        AssumeSorted, DontAssumeSorted
    };

public:
    template<RandomAccessInputIterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void sort_find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it)
    {
        package_merge<DontAssumeSorted>(freq_it, freq_it_end, code_len_it, code_len_limit);
    }

    template<RandomAccessInputIterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void sort_find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it,
            CodeLen custom_limit)
    {
        package_merge<DontAssumeSorted>(freq_it, freq_it_end, code_len_it, custom_limit);
    }

    template<std::input_iterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it)
    {
        package_merge<AssumeSorted>(freq_it, freq_it_end, code_len_it, code_len_limit);
    }

    template<std::input_iterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it,
            CodeLen custom_limit)
    {
        package_merge<AssumeSorted>(freq_it, freq_it_end, code_len_it, custom_limit);
    }

    template<std::input_iterator CodeLenIt>
    static bool validate_code_lens(CodeLenIt code_len_it, CodeLenIt code_len_it_end)
    {
        if (code_len_it == code_len_it_end)
            return true;

        std::size_t nr_nz_code_lens = 0;
        unsigned long long sum_code = 0;
        for (; code_len_it != code_len_it_end; ++code_len_it) {
            CodeLen code_len = *code_len_it;
            if (code_len > code_len_limit)
                return false;
            if (code_len == 0)
                continue;
            else
                ++nr_nz_code_lens;
            sum_code += 1ULL << (code_len_limit - *code_len_it);
        }

        return nr_nz_code_lens <= 1 or sum_code == (1ULL << code_len_limit);
    }

    template<std::input_iterator CodeLenIt, RandomAccessOutputIterator<Code> CodeIt>
    static inline void gen_codes(CodeLenIt code_len_it, CodeLenIt code_len_it_end, CodeIt code_it)
    {
        if (code_len_it == code_len_it_end)
            return;

        auto code_lens = get_sorted_code_lens(code_len_it, code_len_it_end);

        std::size_t i = 0;
        for (; i < code_lens.size() && code_lens[i].first == 0; ++i); // skip 0 code lengths
        if (i == code_lens.size())
            return;

        Code prev_code {};
        *(code_it + code_lens[i].second) = prev_code; // set the shortest non-zero-length code to 0

        for (++i; i < code_lens.size(); ++i) {
            const auto [code_len, code_idx] = code_lens[i];
            const auto [prev_code_len, _] = code_lens[i - 1];
            const Code code = Code(prev_code.to_ullong() + 1) << (code_len - prev_code_len);
            *(code_it + code_idx) = code;
            prev_code = code;
        }
    }

private:
    template<SortAssume sort_assume,
        std::input_iterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static void package_merge(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it, CodeLen depth)
    {
        if (freq_it == freq_it_end)
            return;

        const PackSet packset_per_level = package_freqs<sort_assume>(freq_it, freq_it_end);
        switch (packset_per_level.size()) {
        case 1:
            ++*(code_len_it + packset_per_level.front().symset.front());
        case 0:
            return;
        default:
            break;
        }

        if (packset_per_level.size() > (1 << depth))
            throw Exception<Huffman>("too many symbols or too small depth");

        PackSet packset;
        while (depth--) {
            package(packset);
            merge(packset, packset_per_level);
        }
        calc_length(packset, packset_per_level.size(), code_len_it);
    }

    template<SortAssume sort_assume, std::input_iterator FreqIt>
    static PackSet package_freqs(FreqIt freq_it, FreqIt freq_it_end)
    {
        PackSet packset;
        for (Symset::value_type sym = 0; freq_it != freq_it_end; ++freq_it, ++sym)
            if (*freq_it != 0) // ignore 0 frequencies, code lengths for them will be set to 0
                packset.emplace_back(*freq_it, Symset{static_cast<Symset::value_type>(sym)});
        if constexpr (sort_assume == DontAssumeSorted)
            std::sort(packset.begin(), packset.end(), compare_packs);
        return packset;
    }

    static void package(PackSet& packset)
    {
        for (size_t i = 1; i < packset.size(); i += 2)
            packset[i / 2] = Pack(packset[i - 1], packset[i]);
        packset.erase(packset.begin() + (packset.size() / 2), packset.end());
    }

    static void merge(PackSet& packset, const PackSet& packset_per_level)
    {
        packset.reserve(packset.size() + packset_per_level.size());
        std::copy(packset_per_level.begin(), packset_per_level.end(), std::back_inserter(packset));
        std::inplace_merge(packset.begin(), packset.end() - packset_per_level.size(), packset.end(),
                compare_packs);
    }

    template<RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static void calc_length(const PackSet& packset, std::size_t nr_syms, CodeLenIt code_len_it)
    {
        std::fill_n(code_len_it, nr_syms, 0);
        std::size_t nr_packs = 2 * nr_syms - 2;
        for (std::size_t i = 0; i < nr_packs; ++i) {
            for (auto sym : packset[i].symset) {
                ++*(code_len_it + sym);
            }
        }
    }

    template<RandomAccessInputIterator CodeLenIt>
    static std::vector<std::pair<CodeLen, std::size_t>>
        get_sorted_code_lens(CodeLenIt code_len_it, CodeLenIt code_len_it_end)
    {
        std::vector<std::pair<CodeLen, std::size_t>> code_lens;
        code_lens.reserve(std::distance(code_len_it, code_len_it_end));
        for (; code_len_it != code_len_it_end; ++code_len_it)
            code_lens.emplace_back(*code_len_it, code_lens.size());
        std::sort(code_lens.begin(), code_lens.end());
        return code_lens;
    }

    inline static bool compare_packs(const Pack& left, const Pack& right)
    {
        return left.freq < right.freq;
    }
};

template<HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>>
    requires (Policy::code_len_limit == 15 and CodeLenPolicy::code_len_limit == 7)
class DeflateHuffman {
public:
    using Freq = typename Policy::Freq;
    using CodeLen = typename Policy::CodeLen;
    static constexpr CodeLen code_len_limit = Policy::code_len_limit;
    using Code = std::bitset<code_len_limit>;

    using CodeLenHuffman = Huffman<CodeLenPolicy>;
    using CodeLenFreq = typename CodeLenHuffman::Freq;
    using CodeLenCodeLen = typename CodeLenHuffman::CodeLen;
    using CodeLenCode = typename CodeLenHuffman::Code;

    static constexpr std::size_t code_len_alphabet_size = 19;

    static constexpr unsigned char code_len_alphabet[code_len_alphabet_size] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };

    static constexpr unsigned char code_len_indices[code_len_alphabet_size] = {
        3, 17, 15, 13, 11, 9, 7, 5, 4, 6, 8, 10, 12, 14, 16, 18, 0, 1, 2
    };

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator, typename OutItEnd = void>
    class Encoder;

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::input_iterator InIt = typename std::vector<Char>::iterator, typename InItEnd = void>
    class Decoder;

    static constexpr std::size_t min_nr_code_len_codes = 4;

public:
    template<std::input_iterator CodeLenIt, RandomAccessOutputIterator<CodeLenCodeLen> CodeLenCodeLenIt>
    static void gen_code_len_code_lens(CodeLenIt code_len_it, CodeLenIt code_len_it_end,
            CodeLenCodeLenIt clcl_it)
    {
        CodeLenFreq code_len_freqs[code_len_alphabet_size] {};
        count_code_len_freqs(code_len_it, code_len_it_end, code_len_freqs);
        CodeLenHuffman::sort_find_code_lengths(
                std::begin(code_len_freqs), std::end(code_len_freqs), clcl_it);
    }

    template<std::input_iterator CodeLenCodeLenIt>
    inline static bool validate_code_len_code_lens(CodeLenCodeLenIt clcl_it, CodeLenCodeLenIt clcl_it_end)
    {
        return CodeLenHuffman::validate_code_lens(clcl_it, clcl_it_end);
    }

    template<std::input_iterator CodeLenCodeLenIt, RandomAccessOutputIterator<CodeLenCode> CodeLenCodeIt>
    inline static void gen_code_len_codes(CodeLenCodeLenIt clcl_it, CodeLenCodeLenIt clcl_it_end,
            CodeLenCodeIt clc_it)
    {
        return CodeLenHuffman::gen_codes(clcl_it, clcl_it_end, clc_it);
    }

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator, typename OutItEnd = void>
    inline static auto make_encoder(misc::BitEncoder<Char, char_size, OutIt, OutItEnd>& bit_encoder)
    {
        return Encoder<Char, char_size, OutIt, OutItEnd>(bit_encoder);
    }

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::input_iterator InIt = typename std::vector<Char>::iterator, typename InItEnd = void>
    inline static auto make_decoder(misc::BitDecoder<Char, char_size, InIt, InItEnd>& bit_decoder)
    {
        return Decoder<Char, char_size, InIt, InItEnd>(bit_decoder);
    }

private:
    template<std::input_iterator CodeLenIt>
    static inline std::pair<std::size_t, CodeLen> count_code_len_freq(
            CodeLen curr_len, CodeLenIt& code_len_it, CodeLenIt code_len_it_end)
    {
        std::size_t nr_reps = 1;
        for (; code_len_it != code_len_it_end; ++code_len_it) {
            CodeLen next_len = *code_len_it;
            if (next_len != curr_len)
                return std::make_pair(nr_reps, next_len);
            ++nr_reps;
        }
        return std::make_pair(nr_reps, curr_len);
    }

    template<std::input_iterator CodeLenIt, RandomAccessOutputIterator<CodeLenFreq> CodeLenFreqIt>
    static void count_code_len_freqs(CodeLenIt code_len_it, CodeLenIt code_len_it_end,
            CodeLenFreqIt code_len_freq_it)
    {
        if (code_len_it == code_len_it_end)
            return;

        while (true) {
            CodeLen curr_len = *code_len_it; ++code_len_it;
            assert(curr_len < 16 && "code length exceeding 15");
            auto [nr_reps, next_len] = count_code_len_freq(curr_len, code_len_it, code_len_it_end);
            update_code_len_sym_freqs(curr_len, nr_reps, code_len_freq_it[code_len_indices[curr_len]],
                    code_len_freq_it[0], code_len_freq_it[1], code_len_freq_it[2]);
            if (next_len == curr_len)
                break;
        }
    }

    static void update_code_len_sym_freqs(CodeLen code_len, std::size_t nr_reps,
            CodeLenFreq& code_len_sym, CodeLenFreq& sym16, CodeLenFreq& sym17, CodeLenFreq& sym18)
    {
        if (0 == code_len) {
            sym18 += nr_reps / 138;
            sym18 += nr_reps % 138 >= 11;
            sym17 += nr_reps % 138 >= 3;
            code_len_sym += nr_reps % 138 < 3 ? nr_reps % 138 : 0;
        } else {
            code_len_sym += nr_reps > 0;
            nr_reps -= nr_reps > 0;
            sym16 += (nr_reps / 6) + (nr_reps % 6 >= 3);
            code_len_sym += nr_reps % 6 < 3 ? nr_reps % 6 : 0;
        }
    }
};

template<HuffmanPolicy Policy, HuffmanPolicy CodeLenPolicy>
    requires (Policy::code_len_limit == 15 and CodeLenPolicy::code_len_limit == 7)
template<typename Char, std::size_t char_size, std::output_iterator<Char> OutIt, typename OutItEnd>
class DeflateHuffman<Policy, CodeLenPolicy>::Encoder {
public:
    using BitEncoder = misc::BitEncoder<Char, char_size, OutIt, OutItEnd>;

    explicit Encoder(BitEncoder& bit_encoder) : bit_encoder(bit_encoder)
    {
    }

    inline std::size_t encode_nr_code_len_codes(std::size_t n)
    {
        assert(n >= min_nr_code_len_codes);
        assert(n <= min_nr_code_len_codes + 15);
        return bit_encoder.encode_bits(std::bitset<4>(n - 4));
    }

    inline std::size_t encode_nr_codes(std::size_t n)
    {
        return bit_encoder.encode_bits(std::bitset<5>(n));
    }

    template<RandomAccessInputIterator CodeLenCodeLenIt>
    std::size_t encode_code_len_code_lens(CodeLenCodeLenIt clcl_it, CodeLenCodeLenIt clcl_it_end)
    {
        std::size_t nr_bits_left = 0;
        for (; clcl_it != clcl_it_end; ++clcl_it)
            nr_bits_left += bit_encoder.encode_bits(std::bitset<3>(*clcl_it));
        return nr_bits_left;
    }

    template<RandomAccessInputIterator CodeLenCodeIt>
    std::size_t encode_code_len_sym(CodeLenCodeIt clc_it, std::size_t nr_reps, CodeLen code_len)
    {
        std::size_t nr_bits_left = 0;
        const CodeLenCode clc = clc_it[code_len_indices[code_len]];

        if (0 == code_len) {
            while (nr_reps >= 138) {
                bit_encoder.encode_bits(std::bitset<12>(0x97f));
                nr_reps -= 138;
            }

            if (nr_reps >= 11)
                nr_bits_left += bit_encoder.encode_bits(std::bitset<12>(0x900 | (nr_reps - 11)));
            else if (nr_reps >= 3)
                nr_bits_left += bit_encoder.encode_bits(std::bitset<8>(0x88  | (nr_reps - 3)));
        } else {
            nr_bits_left += bit_encoder.encode_bits(clc);
            --nr_reps;
            while (nr_reps >= 6) {
                nr_bits_left += bit_encoder.encode_bits(std::bitset<7>(0x43));
                nr_reps -= 6;
            }
            if (nr_reps >= 3)
                nr_bits_left += bit_encoder.encode_bits(std::bitset<7>(0x40 | (nr_reps - 3)));
        }

        switch (nr_reps) {
        case 2:
            nr_bits_left += bit_encoder.encode_bits(clc);
        case 1:
            nr_bits_left += bit_encoder.encode_bits(clc);
        default:
            break;
        }

        return nr_bits_left;
    }

    template<RandomAccessInputIterator CodeLenCodeIt, std::input_iterator CodeLenIt>
    std::size_t encode_code_len_syms(CodeLenCodeIt clc_it, CodeLenIt cl_it, CodeLenIt cl_it_end)
    {
        if (cl_it == cl_it_end)
            return 0;
        std::size_t nr_bits_left = 0;

        while (true) {
            CodeLen curr_len = *cl_it; ++cl_it;
            auto [nr_reps, next_len] = count_code_len_freq(curr_len, cl_it, cl_it_end);
            nr_bits_left += encode_code_len_sym(clc_it, nr_reps, curr_len);
            if (curr_len == next_len)
                break;
        }

        return nr_bits_left;
    }

private:
    BitEncoder& bit_encoder;
};

template<HuffmanPolicy Policy, HuffmanPolicy CodeLenPolicy>
    requires (Policy::code_len_limit == 15 and CodeLenPolicy::code_len_limit == 7)
template<typename Char, std::size_t char_size, std::input_iterator InIt, typename InItEnd>
class DeflateHuffman<Policy, CodeLenPolicy>::Decoder {
public:
    using BitDecoder = misc::BitDecoder<Char, char_size, InIt, InItEnd>;

    explicit Decoder(BitDecoder& bit_decoder) : bit_decoder(bit_decoder)
    {
    }

    inline std::size_t decode_nr_code_len_codes(std::size_t& n)
    {
        std::bitset<4> bits;
        const std::size_t nr_bits_left = bit_decoder.decode_bits(bits);
        n = bits.to_ullong() + min_nr_code_len_codes;
        return nr_bits_left;
    }

    inline std::size_t decode_nr_codes(std::size_t& n)
    {
        std::bitset<5> bits;
        const std::size_t nr_bits_left = bit_decoder.decode_bits(bits);
        n = bits.to_ullong();
        return nr_bits_left;
    }

    template<std::output_iterator<CodeLenCodeLen> CodeLenCodeLenIt>
    std::size_t decode_code_len_code_lens(CodeLenCodeLenIt clcl_it, CodeLenCodeLenIt clcl_it_end)
    {
        std::size_t nr_bits_left = 0;
        for (; clcl_it != clcl_it_end; ++clcl_it) {
            std::bitset<3> bits;
            nr_bits_left += bit_decoder.decode_bits(bits);
            *clcl_it = bits.to_ulong();
        }
        return nr_bits_left;
    }

    std::size_t decode_code_len_sym(HuffmanTreeNode *node, std::size_t& nr_reps, CodeLen& code_len)
    {
        std::size_t nr_bits_left = 0;
        while (not node->is_leaf()) {
            std::bitset<1> bit;
            nr_bits_left += bit_decoder.decode_bits(bit);
            node = bit.none() ? node->get_left() : node->get_right();
        }

        if (node->get_symbol() >= code_len_alphabet_size)
            throw Exception<Decoder>("invalid symbol index in a Huffman tree node");

        CodeLen code_len_sym = code_len_alphabet[node->get_symbol()];
        switch (code_len_sym) {
        case 0x10:
        {
            std::bitset<2> extra_bits;
            nr_bits_left += bit_decoder.decode_bits(extra_bits);
            nr_reps = extra_bits.to_ullong();
            break;
        }
        case 0x11:
        {
            std::bitset<3> extra_bits;
            nr_bits_left += bit_decoder.decode_bits(extra_bits);
            code_len = 0;
            nr_reps = extra_bits.to_ullong();
            break;
        }
        case 0x12:
        {
            std::bitset<7> extra_bits;
            nr_bits_left += bit_decoder.decode_bits(extra_bits);
            code_len = 0;
            nr_reps = extra_bits.to_ullong();
            break;
        }
        default:
            code_len = code_len_sym;
            nr_reps = 1;
            break;
        }

        return nr_bits_left;
    }

    template<std::output_iterator<CodeLen> CodeLenIt>
    std::size_t decode_code_len_syms(HuffmanTreeNode *tree_root, CodeLenIt cl_it, CodeLenIt cl_it_end)
    {
        std::size_t nr_bits_left = 0;
        std::size_t nr_reps = 0; CodeLen code_len = 0;
        while (cl_it != cl_it_end && !(nr_bits_left += decode_code_len_sym(tree_root, nr_reps, code_len))) {
            for (; nr_reps != 0 && cl_it != cl_it_end; --nr_reps, ++cl_it)
                *cl_it = code_len;
        }
        return nr_bits_left;
    }

private:
    BitDecoder& bit_decoder;
};

}
