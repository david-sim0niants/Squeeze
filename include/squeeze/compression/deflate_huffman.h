// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <array>

#include "huffman.h"
#include "squeeze/status.h"

namespace squeeze::compression {

/** DeflateHuffman interface defines an additional set of Huffman coding functionalities
 * as specified by the DEFLATE specification (RFC1951).
 * Provides methods for efficiently encoding the generated Huffman code lengths
 * (for generating canonical codes) using another layer of Huffman coding.
 *
 * The code lengths are required to be limited by 15 bits, while the code lengths of codes
 * used for encoding the code lengths themselves shall be limited to 7 bits per the specification.
 *
 * Thus this class is defined by two Huffman policies, one that is used for data encoding/decoding
 * with a code length limit of 15 and the other one used for code lengths encoding/decoding
 * with a code length limit of 7. */
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

    /** The code length alphabet in the order as specified by the spec.
     * 16, 17, 18 symbols are used for denoting repetitions,
     * while the rest of symbols are the all possible code lengths. */
    static constexpr unsigned char code_len_alphabet[code_len_alphabet_size] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };

    /** Reverse mapping of the code length alphabet. Maps symbols to their indices in the alphabet.
     * (for every i in the range of [0, 18] code_len_alphabet[code_len_indices[i]] == i) */
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
    /** Find optimal 7-bit-limited code lengths for encoding the 15-bit-limited code lengths. */
    template<std::input_iterator CodeLenIt, RandomAccessOutputIterator<CodeLenCodeLen> CodeLenCodeLenIt>
    static void find_code_len_code_lens(CodeLenIt code_len_it, CodeLenIt code_len_it_end,
            CodeLenCodeLenIt clcl_it)
    {
        std::array<CodeLenFreq, code_len_alphabet_size> code_len_freqs {};
        count_code_len_freqs(code_len_it, code_len_it_end, code_len_freqs.begin());
        CodeLenHuffman::template find_code_lengths<code_len_alphabet_size>(code_len_freqs.data(), clcl_it);
    }

    /** Validate code lengths of the code lengths alphabet. */
    template<std::input_iterator CodeLenCodeLenIt>
    inline static bool validate_code_len_code_lens(CodeLenCodeLenIt clcl_it, CodeLenCodeLenIt clcl_it_end)
    {
        return CodeLenHuffman::validate_code_lens(clcl_it, clcl_it_end);
    }

    /** Generate codes for the code length alphabet. */
    template<std::input_iterator CodeLenCodeLenIt, RandomAccessOutputIterator<CodeLenCode> CodeLenCodeIt>
    inline static void gen_code_len_codes(CodeLenCodeLenIt clcl_it, CodeLenCodeLenIt clcl_it_end,
            CodeLenCodeIt clc_it)
    {
        return CodeLenHuffman::template gen_codes<code_len_alphabet_size>(clcl_it, clcl_it_end, clc_it);
    }

    /** Make an encoder using the given bit encoder. */
    template<typename Char, std::size_t char_size, std::output_iterator<Char> OutIt, typename OutItEnd>
    inline static auto make_encoder(misc::BitEncoder<Char, char_size, OutIt, OutItEnd>& bit_encoder)
    {
        return Encoder<Char, char_size, OutIt, OutItEnd>(bit_encoder);
    }

    /** Make a decoder using the given bit decoder. */
    template<typename Char, std::size_t char_size, std::input_iterator InIt, typename InItEnd>
    inline static auto make_decoder(misc::BitDecoder<Char, char_size, InIt, InItEnd>& bit_decoder)
    {
        return Decoder<Char, char_size, InIt, InItEnd>(bit_decoder);
    }

private:
    /** Count frequency of the current code length. */
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

    /** Count the frequency of each code length. */
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

    /** Count and update the frequencies of the current code length and the special 16/17/18 symbols. */
    static void update_code_len_sym_freqs(CodeLen code_len, std::size_t nr_reps,
            CodeLenFreq& code_len_sym, CodeLenFreq& sym16, CodeLenFreq& sym17, CodeLenFreq& sym18)
    {
        // extra bits of symbol '16' can encode 3-6 repetitions of the previous non-zero code length
        // extra bits of symbol '17' can encode 3-10  repetitions of zero code lengths
        // extra bits of symbol '18' can encode 11-138  repetitions of zero code lengths
        // for repetitions less than 3, the code length is simply repeated

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

/** DeflateHuffman::Encoder class. Provides methods for encoding code lengths. */
template<HuffmanPolicy Policy, HuffmanPolicy CodeLenPolicy>
    requires (Policy::code_len_limit == 15 and CodeLenPolicy::code_len_limit == 7)
template<typename Char, std::size_t char_size, std::output_iterator<Char> OutIt, typename OutItEnd>
class DeflateHuffman<Policy, CodeLenPolicy>::Encoder {
public:
    using BitEncoder = misc::BitEncoder<Char, char_size, OutIt, OutItEnd>;

    explicit Encoder(BitEncoder& bit_encoder) : bit_encoder(bit_encoder)
    {
    }

    /** Encode the number of code length codes. Encodes (n - 4) in a 4 bit integer code. */
    inline bool encode_nr_code_len_codes(std::size_t n)
    {
        assert(n >= min_nr_code_len_codes);
        assert(n <= min_nr_code_len_codes + 15);
        return bit_encoder.encode_bits(std::bitset<4>(n - 4));
    }

    /** Encode code lengths for the code length alphabet. */
    template<std::input_iterator CodeLenCodeLenIt>
    CodeLenCodeLenIt encode_code_len_code_lens(CodeLenCodeLenIt clcl_it, CodeLenCodeLenIt clcl_it_end)
    {
        for (; clcl_it != clcl_it_end && bit_encoder.encode_bits(std::bitset<3>(*clcl_it)); ++clcl_it);
        return clcl_it;
    }

    /** Encode a single code length with its repetitions. */
    template<RandomAccessInputIterator CodeLenCodeIt, RandomAccessInputIterator CodeLenCodeLenIt>
    bool encode_code_rep_len(CodeLenCodeIt clc_it, CodeLenCodeLenIt clcl_it,
            std::size_t nr_reps, CodeLen code_len)
    {
        bool s = true;
        const CodeLenCode clc = clc_it[code_len_indices[code_len]];
        const CodeLenCodeLen clcl = clcl_it[code_len_indices[code_len]];

        if (0 == code_len) {
            const CodeLenCode clc18 = clc_it[code_len_indices[0x12]];
            const CodeLenCodeLen clcl18 = clcl_it[code_len_indices[0x12]];
            const CodeLenCode clc17 = clc_it[code_len_indices[0x11]];
            const CodeLenCodeLen clcl17 = clcl_it[code_len_indices[0x11]];

            while (s && nr_reps >= 138) {
                s = bit_encoder.encode_bits(clc18, clcl18) &&
                    bit_encoder.encode_bits(std::bitset<7>(0x7F));
                nr_reps -= 138;
            }

            if (s && nr_reps >= 11) {
                s = bit_encoder.encode_bits(clc18, clcl18) &&
                    bit_encoder.encode_bits(std::bitset<7>(nr_reps - 11));
            }
            else if (s && nr_reps >= 3) {
                s = bit_encoder.encode_bits(clc17, clcl17) &&
                    bit_encoder.encode_bits(std::bitset<3>(nr_reps - 3));
            }
        } else {
            const CodeLenCode clc16 = clc_it[code_len_indices[0x10]];
            const CodeLenCodeLen clcl16 = clcl_it[code_len_indices[0x10]];

            bit_encoder.encode_bits(clc, clcl);
            --nr_reps;
            while (s && nr_reps >= 6) {
                s = bit_encoder.encode_bits(clc16, clcl16) &&
                    bit_encoder.encode_bits(std::bitset<2>(3));
                nr_reps -= 6;
            }
            if (s && nr_reps >= 3) {
                s = bit_encoder.encode_bits(clc16, clcl16) &&
                    bit_encoder.encode_bits(std::bitset<2>(nr_reps - 3));
            }
        }

        switch (nr_reps) {
        case 2:
            s = bit_encoder.encode_bits(clc, clcl);
        case 1:
            s = bit_encoder.encode_bits(clc, clcl);
        default:
            break;
        }

        return s;
    }

    /** Encode code length symbols. */
    template<RandomAccessInputIterator CodeLenCodeIt, RandomAccessInputIterator CodeLenCodeLenIt,
        std::input_iterator CodeLenIt>
    CodeLenIt encode_code_len_syms(CodeLenCodeIt clc_it, CodeLenCodeLenIt clcl_it,
            CodeLenIt cl_it, CodeLenIt cl_it_end)
    {
        if (cl_it == cl_it_end)
            return cl_it;

        while (true) {
            CodeLen curr_len = *cl_it; ++cl_it;
            auto [nr_reps, next_len] = count_code_len_freq(curr_len, cl_it, cl_it_end);
            if (not encode_code_rep_len(clc_it, clcl_it, nr_reps, curr_len) or curr_len == next_len)
                break;
        }

        return cl_it;
    }

    /** Encode code lengths. */
    template<std::forward_iterator CodeLenIt>
    StatStr encode_code_lens(CodeLenIt cl_it, CodeLenIt cl_it_end)
    {
        std::array<CodeLenCodeLen, code_len_alphabet_size> clcl {};
        find_code_len_code_lens(cl_it, cl_it_end, clcl.begin());

        std::array<CodeLenCode, code_len_alphabet_size> clc {};
        gen_code_len_codes(clcl.begin(), clcl.end(), clc.begin());

        std::size_t clcl_size = clcl.size();
        for (; clcl_size > min_nr_code_len_codes && clcl[clcl_size - 1] == 0; --clcl_size);

        bool s = encode_nr_code_len_codes(clcl_size);
        if (not s) [[unlikely]]
            return "failed encoding number of code length codes";

        auto clcl_it = clcl.begin(); auto clcl_it_end = clcl_it + clcl_size;
        clcl_it = encode_code_len_code_lens(clcl.begin(), clcl.begin() + clcl_size);
        if (clcl_it != clcl_it_end) [[unlikely]]
            return "failed encoding code lengths for the code length alphabet";

        cl_it = encode_code_len_syms(clc.begin(), clcl.begin(), cl_it, cl_it_end);
        if (cl_it != cl_it_end) [[unlikely]]
            return "failed encoding code lengths";

        return success;
    }

private:
    BitEncoder& bit_encoder;
};

/** DeflateHuffman::Decoder class. Provides methods for decoding code lengths. */
template<HuffmanPolicy Policy, HuffmanPolicy CodeLenPolicy>
    requires (Policy::code_len_limit == 15 and CodeLenPolicy::code_len_limit == 7)
template<typename Char, std::size_t char_size, std::input_iterator InIt, typename InItEnd>
class DeflateHuffman<Policy, CodeLenPolicy>::Decoder {
public:
    using BitDecoder = misc::BitDecoder<Char, char_size, InIt, InItEnd>;

    explicit Decoder(BitDecoder& bit_decoder) : bit_decoder(bit_decoder)
    {
    }

    /** Decode the number of code length codes. Returns (decoded 4 bit integer + 4). */
    inline bool decode_nr_code_len_codes(std::size_t& n)
    {
        std::bitset<4> bits;
        bool s = bit_decoder.decode_bits(bits);
        n = bits.to_ullong() + min_nr_code_len_codes;
        return s;
    }

    /** Decode code lengths for the code length alphabet. */
    template<std::output_iterator<CodeLenCodeLen> CodeLenCodeLenIt>
    CodeLenCodeLenIt decode_code_len_code_lens(CodeLenCodeLenIt clcl_it, CodeLenCodeLenIt clcl_it_end)
    {
        std::bitset<3> bits;
        for (; clcl_it != clcl_it_end && bit_decoder.decode_bits(bits); ++clcl_it)
            *clcl_it = bits.to_ulong();
        return clcl_it;
    }

    /** Decode a single code length with its repetitions. */
    bool decode_code_len_sym(const HuffmanTreeNode *tree_root, std::size_t& nr_reps, CodeLen& code_len)
    {
        std::optional<unsigned int> sym_idx = tree_root->decode_sym_from(bit_decoder);
        if (!sym_idx.has_value())
            return false;

        bool s = true;
        CodeLen code_len_sym = code_len_alphabet[*sym_idx];
        switch (code_len_sym) {
        case 0x10:
        {
            std::bitset<2> extra_bits;
            s = bit_decoder.decode_bits(extra_bits);
            nr_reps = extra_bits.to_ullong() + 3;
            break;
        }
        case 0x11:
        {
            std::bitset<3> extra_bits;
            s = bit_decoder.decode_bits(extra_bits);
            code_len = 0;
            nr_reps = extra_bits.to_ullong() + 3;
            break;
        }
        case 0x12:
        {
            std::bitset<7> extra_bits;
            s = bit_decoder.decode_bits(extra_bits);
            code_len = 0;
            nr_reps = extra_bits.to_ullong() + 11;
            break;
        }
        default:
            code_len = code_len_sym;
            nr_reps = 1;
            break;
        }

        return s;
    }

    /** Decode code length symbols. */
    template<std::output_iterator<CodeLen> CodeLenIt>
    CodeLenIt decode_code_len_syms(const HuffmanTreeNode *tree_root, CodeLenIt cl_it, CodeLenIt cl_it_end)
    {
        std::size_t nr_reps = 0; CodeLen code_len = 0;
        while (cl_it != cl_it_end && decode_code_len_sym(tree_root, nr_reps, code_len))
            for (; nr_reps != 0 && cl_it != cl_it_end; --nr_reps, ++cl_it)
                *cl_it = code_len;
        return cl_it;
    }

    /** Decode code lengths. */
    template<std::output_iterator<CodeLen> CodeLenIt>
    StatStr decode_code_lens(CodeLenIt cl_it, CodeLenIt cl_it_end)
    {
        std::size_t clcl_size = 0;
        bool s = decode_nr_code_len_codes(clcl_size);
        if (not s) [[unlikely]]
            return "failed decoding number of code lengths codes";

        std::array<CodeLenCodeLen, code_len_alphabet_size> clcl {};
        auto clcl_it = clcl.begin(); auto clcl_it_end = clcl_it + clcl_size;

        clcl_it = decode_code_len_code_lens(clcl_it, clcl_it_end);
        if (clcl_it != clcl_it_end) [[unlikely]]
            return "failed decoding code lengths for the code length alphabet";
        else if (not validate_code_len_code_lens(clcl.begin(), clcl_it_end)) [[unlikely]]
            return "invalid code lengths for the code length alphabet decoded";

        std::array<CodeLenCode, code_len_alphabet_size> clc {};
        gen_code_len_codes(clcl.begin(), clcl.begin() + clcl_size, clc.begin());
        HuffmanTree tree;
        tree.build_from_codes(clc.begin(),  clc.begin() + clcl_size,
                             clcl.begin(), clcl.begin() + clcl_size);

        cl_it = decode_code_len_syms(tree.get_root(), cl_it, cl_it_end);
        if (cl_it != cl_it_end) [[unlikely]]
            return "failed decoding code lengths";
        else
            return success;
    }

private:
    BitDecoder& bit_decoder;
};

}
