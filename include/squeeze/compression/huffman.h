// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <bitset>
#include <vector>
#include <algorithm>
#include <cassert>

#include "huffman_policy.h"
#include "huffman_tree.h"
#include "huffman_package_merge.h"
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

    using PackageMerge = HuffmanPackageMerge<Freq, CodeLen>;

    template<typename Char, std::size_t char_size,
        RandomAccessInputIterator CodeLenIt, RandomAccessInputIterator CodeIt,
        std::output_iterator<Char> OutIt, typename... OutItEnd>
    class Encoder;

    template<typename Char, std::size_t char_size, std::input_iterator InIt, typename... InItEnd>
    class Decoder;

public:
    /* Find code lengths for each frequency. */
    template<std::input_iterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void find_code_lengths(FreqIt freq_it, std::size_t nr_freqs, CodeLenIt code_len_it)
    {
        PackageMerge::template package_merge<code_len_limit>(freq_it, nr_freqs, code_len_it);
    }

    /* Find code lengths for each frequency. Assumes the number of frequencies is compile-time constant. */
    template<std::size_t nr_freqs, std::input_iterator FreqIt,
        RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void find_code_lengths(FreqIt freq_it, CodeLenIt code_len_it)
    {
        PackageMerge::template package_merge<code_len_limit, nr_freqs>(freq_it, code_len_it);
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

    /* Generate canonical Huffman codes based on the provided code lengths. */
    template<RandomAccessInputIterator CodeLenIt, RandomAccessOutputIterator<Code> CodeIt>
    static inline void gen_codes(CodeLenIt code_len_it, CodeLenIt code_len_it_end, CodeIt code_it)
    {
        const std::size_t nr_codes = std::distance(code_len_it, code_len_it_end);
        std::vector<std::pair<CodeLen, std::size_t>> cl_idx_pairs_storage(nr_codes);
        gen_codes(code_len_it, code_it, cl_idx_pairs_storage);
    }

    /* Generate canonical Huffman codes based on the provided code lengths.
     * Assumes that the max number of codes in general (usually the alphabet size) is compile-time constant.
     * This helps avoiding dynamic memory allocation. */
    template<std::size_t max_nr_codes,
        RandomAccessInputIterator CodeLenIt, RandomAccessOutputIterator<Code> CodeIt>
    static inline void gen_codes(CodeLenIt code_len_it, CodeLenIt code_len_it_end, CodeIt code_it)
    {
        const std::size_t nr_codes = std::distance(code_len_it, code_len_it_end);
        std::array<std::pair<CodeLen, std::size_t>, max_nr_codes> cl_idx_pairs_storage;
        gen_codes(code_len_it, code_it, std::span(cl_idx_pairs_storage.data(), nr_codes));
    }

    /* Make an encoder using the given bit encoder. */
    template<typename Char, std::size_t char_size,
        RandomAccessInputIterator CodeLenIt, RandomAccessInputIterator CodeIt,
        std::output_iterator<Char> OutIt, typename... OutItEnd>
    inline static auto make_encoder(CodeIt code_it, CodeLenIt code_len_it,
            misc::BitEncoder<Char, char_size, OutIt, OutItEnd...>& bit_encoder)
    {
        return Encoder<Char, char_size, CodeLenIt, CodeIt, OutIt, OutItEnd...>
            (code_it, code_len_it, bit_encoder);
    }

    /* Make an decoder using the given bit decoder. */
    template<typename Char, std::size_t char_size, std::input_iterator InIt, typename... InItEnd>
    inline static auto make_decoder(const HuffmanTreeNode *huffman_tree_root,
            misc::BitDecoder<Char, char_size, InIt, InItEnd...>& bit_decoder)
    {
        return Decoder<Char, char_size, InIt, InItEnd...>(huffman_tree_root, bit_decoder);
    }

private:
    template<RandomAccessInputIterator CodeLenIt, RandomAccessOutputIterator<Code> CodeIt>
    static inline void gen_codes(CodeLenIt code_len_it, CodeIt code_it,
            std::span<std::pair<CodeLen, std::size_t>> cl_idx_pairs)
    {
        if (cl_idx_pairs.empty())
            return;

        get_sorted_code_lens(code_len_it, cl_idx_pairs);

        std::size_t i = 0;
        for (; i < cl_idx_pairs.size() && cl_idx_pairs[i].first == 0; ++i); // skip 0 code lengths
        if (i == cl_idx_pairs.size())
            return;

        Code prev_code {};
        *(code_it + cl_idx_pairs[i].second) = prev_code; // set the shortest non-zero-length code to 0

        for (++i; i < cl_idx_pairs.size(); ++i) {
            const auto [code_len, code_idx] = cl_idx_pairs[i];
            const auto [prev_code_len, _] = cl_idx_pairs[i - 1];
            const Code code = Code(prev_code.to_ullong() + 1) << (code_len - prev_code_len);
            *(code_it + code_idx) = code;
            prev_code = code;
        }
    }

    template<RandomAccessInputIterator CodeLenIt>
    static void get_sorted_code_lens(CodeLenIt code_len_it,
            std::span<std::pair<CodeLen, std::size_t>> cl_idx_pairs)
    {
        std::size_t i;
        for (i = 0; i < cl_idx_pairs.size(); ++code_len_it, ++i) {
            cl_idx_pairs[i].first = *code_len_it;
            cl_idx_pairs[i].second = i;
        }
        std::sort(cl_idx_pairs.begin(), cl_idx_pairs.end());
    }
};

/* Huffman::Encoder class. Provides methods for encoding symbols. */
template<HuffmanPolicy Policy>
template<typename Char, std::size_t char_size,
    RandomAccessInputIterator CodeLenIt, RandomAccessInputIterator CodeIt,
    std::output_iterator<Char> OutIt, typename... OutItEnd>
class Huffman<Policy>::Encoder {
public:
    using BitEncoder = misc::BitEncoder<Char, char_size, OutIt, OutItEnd...>;

    explicit Encoder(CodeIt code_it, CodeLenIt code_len_it, BitEncoder& bit_encoder)
        : code_it(code_it), code_len_it(code_len_it), bit_encoder(bit_encoder)
    {
    }

    /* Encode a single symbol. */
    inline bool encode_sym(unsigned int sym_idx)
    {
        return bit_encoder.encode_bits(code_it[sym_idx], code_len_it[sym_idx]);
    }

    /* Encode symbols given the codes, code lengths and an optional
     * symbol-to-index converter that defaults to the identity function. */
    template<std::input_iterator InIt, typename Sym = std::iter_value_t<InIt>,
             std::invocable<Sym> Sym2Idx = unsigned int (*)(Sym)>
    inline InIt encode_syms(InIt in_it, InIt in_it_end, Sym2Idx sym2idx = default_sym2idx<Sym>)
        requires std::convertible_to<std::invoke_result_t<Sym2Idx, Sym>, unsigned int>
    {
        for (; in_it != in_it_end && encode_sym(sym2idx(*in_it)); ++in_it);
        return in_it;
    }

    /* Encode symbols with a termination mark at the end, given the codes, code lengths, terminator
     * index and an optional symbol-to-index converter that defaults to the identity function. */
    template<std::input_iterator InIt, typename Sym = std::iter_value_t<InIt>,
             std::invocable<Sym> Sym2Idx = unsigned int (*)(Sym)>
    InIt encode_syms(InIt in_it, InIt in_it_end, unsigned int term_idx,
            Sym2Idx sym2idx = default_sym2idx<Sym>)
    {
        in_it = encode_syms(in_it, in_it_end, sym2idx);
        encode_sym(term_idx);
        return in_it;
    }

    template<typename Sym>
    inline static unsigned int default_sym2idx(Sym sym)
    {
        return static_cast<unsigned int>(static_cast<std::make_unsigned_t<Sym>>(sym));
    };

private:
    CodeIt code_it;
    CodeLenIt code_len_it;
    BitEncoder& bit_encoder;
};

/* Huffman::Decoder class. Provides methods for decoding symbols. */
template<HuffmanPolicy Policy>
template<typename Char, std::size_t char_size, std::input_iterator InIt, typename... InItEnd>
class Huffman<Policy>::Decoder {
public:
    using BitDecoder = misc::BitDecoder<Char, char_size, InIt, InItEnd...>;

    explicit Decoder(const HuffmanTreeNode *huffman_tree_root, BitDecoder& bit_decoder)
        : huffman_tree_root(huffman_tree_root), bit_decoder(bit_decoder)
    {
    }

    /* Decode a single symbol. */
    inline std::optional<unsigned int> decode_sym()
    {
        return huffman_tree_root->decode_sym_from(bit_decoder);
    }

    /* Decode symbols given the Huffman tree root and an optional index-to-symbol converter
     * that defaults to the identity function. */
    template<typename Sym = char, std::output_iterator<Sym> OutIt,
        std::invocable<unsigned int> Idx2Sym = Sym (*)(unsigned int)>
    OutIt decode_syms(OutIt out_it, OutIt out_it_end,
            Idx2Sym idx2sym = default_idx2sym<Sym>)
        requires std::convertible_to<std::invoke_result_t<Idx2Sym, unsigned int>, Sym>
    {
        return decode_syms<false>(out_it, out_it_end, HuffmanTreeNode::sentinel_symbol, idx2sym);
    }

    /* Decode symbols expecting a termination mark at the end, given the Huffman tree root, terminator
     * index and an optional index-to-symbol converter that defaults to the identity function. */
    template<typename Sym = char, std::output_iterator<Sym> OutIt,
        std::invocable<unsigned int> Idx2Sym = Sym (*)(unsigned int)>
    OutIt decode_syms(OutIt out_it, OutIt out_it_end,
            unsigned int term_idx, Idx2Sym idx2sym = default_idx2sym<Sym>)
        requires std::convertible_to<std::invoke_result_t<Idx2Sym, unsigned int>, Sym>
    {
        return decode_syms<true>(out_it, out_it_end, term_idx, idx2sym);
    }

    template<typename Sym>
    inline static Sym default_idx2sym(unsigned int idx)
    {
        return static_cast<Sym>(static_cast<int>(idx));
    }

private:
    template<bool expect_term, typename Sym = char, std::output_iterator<Sym> OutIt,
        std::invocable<unsigned int> Idx2Sym = Sym (*)(unsigned int)>
    OutIt decode_syms(OutIt out_it, OutIt out_it_end, unsigned int term_idx, Idx2Sym idx2sym)
        requires std::convertible_to<std::invoke_result_t<Idx2Sym, unsigned int>, Sym>
    {
        for (; out_it != out_it_end; ++out_it) {
            std::optional<unsigned int> sym_idx = decode_sym();
            if (!sym_idx.has_value() || expect_term && sym_idx == term_idx)
                break;
            *out_it = idx2sym(*sym_idx);
        }
        return out_it;
    }

    const HuffmanTreeNode *huffman_tree_root;
    BitDecoder& bit_decoder;
};

}
