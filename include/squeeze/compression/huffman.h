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

/* Huffman coding interface defined by its policy rules implementing
 * the essential Huffman coding methods. */
template<HuffmanPolicy Policy = BasicHuffmanPolicy<15>>
class Huffman {
public:
    using Freq = typename Policy::Freq;
    using CodeLen = typename Policy::CodeLen;
    static constexpr CodeLen code_len_limit = Policy::code_len_limit;
    using Code = std::bitset<code_len_limit>;

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator, typename OutItEnd = void>
    class Encoder;

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::input_iterator InIt = typename std::vector<Char>::iterator, typename InItEnd = void>
    class Decoder;

private:
    using Symset = std::vector<unsigned int>;

    /* A pack type used in package-merge algorithm. */
    struct Pack {
        Freq freq; /* Cumulative frequency of all symbols in the symset. */
        Symset symset; /* The symbol set. */

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
    /* Find code lengths for each frequency.
     * This does not assume that frequencies are sorted and handles the sorting itself. */
    template<RandomAccessInputIterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void sort_find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it)
    {
        package_merge<DontAssumeSorted>(freq_it, freq_it_end, code_len_it, code_len_limit);
    }

    /* Find code lengths for each frequency with a custom code length limit.
     * This does not assume that frequencies are sorted and handles the sorting itself. */
    template<RandomAccessInputIterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void sort_find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it,
            CodeLen custom_limit)
    {
        package_merge<DontAssumeSorted>(freq_it, freq_it_end, code_len_it, custom_limit);
    }

    /* Find code lengths for each frequency. This assumes that frequencies are sorted. */
    template<std::input_iterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it)
    {
        package_merge<AssumeSorted>(freq_it, freq_it_end, code_len_it, code_len_limit);
    }

    /* Find code lengths for each frequency with a custom code length limit.
     * This assumes that frequencies are sorted. */
    template<std::input_iterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it,
            CodeLen custom_limit)
    {
        package_merge<AssumeSorted>(freq_it, freq_it_end, code_len_it, custom_limit);
    }

    /* Validate code lengths. Ignores zero lengths.
     * Validation fails if any of the code lengths exceed the code length limit.
     * If there's only one non-zero code length, validation passes.
     * Otherwise it indirectly computes the sum of 2^(-code_length) for all non-zero code lengths.
     * Validation only passes if the sum is 1, or, in other words, if there is a full binary tree
     * with its leaves having the same depths as the non-zero code lengths. */
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

    /* Generate canonical Huffman codes based on the provided the code lengths. */
    template<RandomAccessInputIterator CodeLenIt, RandomAccessOutputIterator<Code> CodeIt>
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

    /* Make an encoder using the given bit encoder. */
    template<typename Char, std::size_t char_size, std::output_iterator<Char> OutIt, typename OutItEnd>
    inline static auto make_encoder(misc::BitEncoder<Char, char_size, OutIt, OutItEnd>& bit_encoder)
    {
        return Encoder<Char, char_size, OutIt, OutItEnd>(bit_encoder);
    }

    /* Make an decoder using the given bit decoder. */
    template<typename Char, std::size_t char_size, std::input_iterator InIt, typename InItEnd>
    inline static auto make_decoder(misc::BitDecoder<Char, char_size, InIt, InItEnd>& bit_decoder)
    {
        return Decoder<Char, char_size, InIt, InItEnd>(bit_decoder);
    }

private:
    /* The package-merge algorithm used for finding length-limited Huffman code lengths
     * for the given set of frequencies. */
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

    /* Makes initial packages containing only one symbol with its corresponding frequency. */
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

    /* Combine every pair of packages into one. Ignore the last package if it got no pair. */
    static void package(PackSet& packset)
    {
        for (size_t i = 1; i < packset.size(); i += 2)
            packset[i / 2] = Pack(packset[i - 1], packset[i]);
        packset.erase(packset.begin() + (packset.size() / 2), packset.end());
    }

    /* Merge a per-level sorted packset into the current sorted packset. */
    static void merge(PackSet& packset, const PackSet& packset_per_level)
    {
        packset.reserve(packset.size() + packset_per_level.size());
        std::copy(packset_per_level.begin(), packset_per_level.end(), std::back_inserter(packset));
        std::inplace_merge(packset.begin(), packset.end() - packset_per_level.size(), packset.end(),
                compare_packs);
    }

    /* Calculate length of each symbol. */
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

/* Huffman::Encoder class. Provides methods for encoding symbols. */
template<HuffmanPolicy Policy>
template<typename Char, std::size_t char_size, std::output_iterator<Char> OutIt, typename OutItEnd>
class Huffman<Policy>::Encoder {
public:
    using BitEncoder = misc::BitEncoder<Char, char_size, OutIt, OutItEnd>;

    explicit Encoder(BitEncoder& bit_encoder) : bit_encoder(bit_encoder)
    {
    }

    /* Encode symbols given the codes, code lengths and an optional
     * symbol-to-index converter that defaults to the identity function. */
    template<RandomAccessInputIterator CodeIt, RandomAccessInputIterator CodeLenIt,
             std::input_iterator InIt, typename Sym = std::iter_value_t<InIt>,
             std::invocable<Sym> Sym2Idx = unsigned int (*)(Sym)>
    InIt encode_syms(CodeIt code_it, CodeLenIt code_len_it, InIt in_it, InIt in_it_end,
            Sym2Idx sym2idx = default_sym2idx<Sym>)
        requires std::convertible_to<std::invoke_result_t<Sym2Idx, Sym>, unsigned int>
    {
        for (; in_it != in_it_end; ++in_it) {
            const auto sym = *in_it;
            const unsigned int idx = sym2idx(sym);
            const std::size_t nr_bits_left = bit_encoder.encode_bits(code_it[idx], code_len_it[idx]);
            if (nr_bits_left)
                break;
        }
        return in_it;
    }

    /* Encode symbols with a termination mark at the end, given the codes, code lengths, terminator
     * index and an optional symbol-to-index converter that defaults to the identity function. */
    template<RandomAccessInputIterator CodeIt, RandomAccessInputIterator CodeLenIt,
             std::input_iterator InIt, typename Sym = std::iter_value_t<InIt>,
             std::invocable<Sym> Sym2Idx = unsigned int (*)(Sym)>
    InIt encode_syms(CodeIt code_it, CodeLenIt code_len_it, InIt in_it, InIt in_it_end,
            unsigned int term_idx, Sym2Idx sym2idx = default_sym2idx<Sym>)
    {
        in_it = encode_syms(code_it, code_len_it, in_it, in_it_end, sym2idx);
        bit_encoder.encode_bits(code_it[term_idx], code_len_it[term_idx]);
        return in_it;
    }

    template<typename Sym>
    inline static unsigned int default_sym2idx(Sym sym)
    {
        return static_cast<unsigned int>(static_cast<std::make_unsigned_t<Sym>>(sym));
    };

private:
    BitEncoder& bit_encoder;
};

/* Huffman::Decoder class. Provides methods for decoding symbols. */
template<HuffmanPolicy Policy>
template<typename Char, std::size_t char_size, std::input_iterator InIt, typename InItEnd>
class Huffman<Policy>::Decoder {
public:
    using BitDecoder = misc::BitDecoder<Char, char_size, InIt, InItEnd>;

    explicit Decoder(BitDecoder& bit_decoder) : bit_decoder(bit_decoder)
    {
    }

    /* Decode symbols given the Huffman tree root and an optional index-to-symbol converter
     * that defaults to the identity function. */
    template<typename Sym = char, std::output_iterator<Sym> OutIt,
        std::invocable<unsigned int> Idx2Sym = Sym (*)(unsigned int)>
    OutIt decode_syms(const HuffmanTreeNode *root, OutIt out_it, OutIt out_it_end,
            Idx2Sym idx2sym = default_idx2sym<Sym>)
        requires std::convertible_to<std::invoke_result_t<Idx2Sym, unsigned int>, Sym>
    {
        return decode_syms<false>(root, out_it, out_it_end, HuffmanTreeNode::sentinel_symbol, idx2sym);
    }

    /* Decode symbols expecting a termination mark at the end, given the Huffman tree root, terminator
     * index and an optional index-to-symbol converter that defaults to the identity function. */
    template<typename Sym = char, std::output_iterator<Sym> OutIt,
        std::invocable<unsigned int> Idx2Sym = Sym (*)(unsigned int)>
    OutIt decode_syms(const HuffmanTreeNode *root, OutIt out_it, OutIt out_it_end,
            unsigned int term_idx, Idx2Sym idx2sym = default_idx2sym<Sym>)
        requires std::convertible_to<std::invoke_result_t<Idx2Sym, unsigned int>, Sym>
    {
        return decode_syms<true>(root, out_it, out_it_end, term_idx, idx2sym);
    }

    template<typename Sym>
    inline static Sym default_idx2sym(unsigned int idx)
    {
        return static_cast<Sym>(static_cast<int>(idx));
    }

private:
    template<bool expect_term, typename Sym = char, std::output_iterator<Sym> OutIt,
        std::invocable<unsigned int> Idx2Sym = Sym (*)(unsigned int)>
    OutIt decode_syms(const HuffmanTreeNode *root, OutIt out_it, OutIt out_it_end,
            unsigned int term_idx, Idx2Sym idx2sym)
        requires std::convertible_to<std::invoke_result_t<Idx2Sym, unsigned int>, Sym>
    {
        for (; out_it != out_it_end; ++out_it) {
            std::size_t nr_bits_left = 0;
            const unsigned int idx = root->find_symbol(bit_decoder.make_bit_reader_iterator(nr_bits_left));
            if (nr_bits_left || expect_term && idx == term_idx)
                break;
            *out_it = idx2sym(idx);
        }
        return out_it;
    }

    BitDecoder& bit_decoder;
};

}
