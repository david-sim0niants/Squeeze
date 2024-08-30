#pragma once

#include <array>
#include <tuple>

#include "huffman.h"
#include "deflate_huffman.h"
#include "squeeze/error.h"

namespace squeeze::compression {

template<HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>>
    requires (Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7)
class Huffman15 {
public:
    using Freq = typename Huffman<Policy>::Freq;
    using CodeLen = typename Huffman<Policy>::CodeLen;
    static constexpr CodeLen code_len_limit = Huffman<Policy>::code_len_limit;
    using Code = typename Huffman<Policy>::Code;

    using DHuffman = DeflateHuffman<Policy, CodeLenPolicy>;
    using CodeLenCodeLen = typename DHuffman::CodeLenCodeLen;
    using CodeLenCode = typename DHuffman::CodeLenCode;

    static constexpr std::size_t alphabet_size = 257;
    static constexpr int term_sym = 0x100;

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator, typename OutItEnd = void>
    class Encoder;

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::input_iterator InIt = typename std::vector<Char>::iterator, typename InItEnd = void>
    class Decoder;

public:
    template<typename Char, std::size_t char_size, std::output_iterator<Char> OutIt, typename OutItEnd>
    inline static auto make_encoder(misc::BitEncoder<Char, char_size, OutIt, OutItEnd>& bit_encoder)
    {
        return Encoder<Char, char_size, OutIt, OutItEnd>(bit_encoder);
    }

    template<typename Char, std::size_t char_size, std::input_iterator InIt, typename InItEnd>
    inline static auto make_decoder(misc::BitDecoder<Char, char_size, InIt, InItEnd>& bit_decoder)
    {
        return Decoder<Char, char_size, InIt, InItEnd>(bit_decoder);
    }

    template<std::input_iterator InIt, RandomAccessOutputIterator<Freq> FreqIt>
    static void count_freqs(InIt in_it, InIt in_it_end, FreqIt freq_it)
        requires std::convertible_to<std::iter_value_t<InIt>, char>
    {
        for (; in_it != in_it_end; ++in_it)
            ++freq_it[static_cast<unsigned char>(*in_it)];
    }
};

template<HuffmanPolicy Policy, HuffmanPolicy CodeLenPolicy>
    requires (Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7)
template<typename Char, std::size_t char_size, std::output_iterator<Char> OutIt, typename OutItEnd>
class Huffman15<Policy, CodeLenPolicy>::Encoder {
public:
    using BitEncoder = misc::BitEncoder<Char, char_size, OutIt, OutItEnd>;

    explicit Encoder(BitEncoder& bit_encoder) : bit_encoder(bit_encoder)
    {
    }

    template<std::forward_iterator CodeLenIt>
    Error<Encoder> encode_code_lens(CodeLenIt cl_it, CodeLenIt cl_it_end)
    {
        std::array<CodeLenCodeLen, DHuffman::code_len_alphabet_size> clcl {};
        DHuffman::gen_code_len_code_lens(cl_it, cl_it_end, clcl.begin());
        std::array<CodeLenCode, DHuffman::code_len_alphabet_size> clc {};
        DHuffman::gen_code_len_codes(clcl.begin(), clcl.end(), clc.begin());

        std::size_t clcl_size = clcl.size();
        for (; clcl_size > DHuffman::min_nr_code_len_codes && clcl[clcl_size - 1] == 0; --clcl_size);

        auto dh_encoder = DHuffman::make_encoder(bit_encoder);

        std::size_t nr_bits_left = dh_encoder.encode_nr_code_len_codes(clcl_size);
        if (nr_bits_left)
            return "failed encoding number of code length codes";

        auto clcl_it = clcl.begin(); auto clcl_it_end = clcl_it + clcl_size;
        clcl_it = dh_encoder.encode_code_len_code_lens(clcl.begin(), clcl.begin() + clcl_size);
        if (clcl_it != clcl_it_end)
            return "failed encoding code lengths for the code length alphabet";

        cl_it = dh_encoder.encode_code_len_syms(clc.begin(), clcl.begin(), cl_it, cl_it_end);
        if (cl_it != cl_it_end)
            return "failed encoding code lengths";

        return success;
    }

    template<bool use_term, std::forward_iterator InIt>
    std::tuple<InIt, Error<Encoder>> encode_data(InIt in_it, InIt in_it_end)
        requires std::convertible_to<std::iter_value_t<InIt>, char>
    {
        std::array<Freq, alphabet_size> freqs {};
        freqs[term_sym] = use_term;

        count_freqs(in_it, in_it_end, freqs.data());
        std::array<CodeLen, alphabet_size> code_lens {};
        Huffman<Policy>::sort_find_code_lengths(freqs.begin(), freqs.end(), code_lens.begin());
        std::array<Code, alphabet_size> codes {};
        Huffman<Policy>::gen_codes(code_lens.begin(), code_lens.end(), codes.data());

        auto e = encode_code_lens(code_lens.begin(), code_lens.end());
        if (e)
            return {in_it, {"failed encoding code lengths", e.report()}};

        auto huffman_encoder = Huffman<Policy>::make_encoder(bit_encoder);
        if constexpr (use_term)
            in_it = huffman_encoder.encode_codes(codes.begin(), code_lens.begin(), in_it, in_it_end, term_sym);
        else
            in_it = huffman_encoder.encode_codes(codes.begin(), code_lens.begin(), in_it, in_it_end);
        return {in_it, success};
    }

private:
    BitEncoder& bit_encoder;
};

template<HuffmanPolicy Policy, HuffmanPolicy CodeLenPolicy>
    requires (Policy::code_len_limit == 15 and CodeLenPolicy::code_len_limit == 7)
template<typename Char, std::size_t char_size, std::input_iterator InIt, typename InItEnd>
class Huffman15<Policy, CodeLenPolicy>::Decoder {
public:
    using BitDecoder = misc::BitDecoder<Char, char_size, InIt, InItEnd>;

    explicit Decoder(BitDecoder& bit_decoder) : bit_decoder(bit_decoder)
    {
    }

    template<std::output_iterator<CodeLen> CodeLenIt>
    Error<Decoder> decode_code_lens(CodeLenIt cl_it, CodeLenIt cl_it_end)
    {
        auto dh_decoder = DHuffman::make_decoder(bit_decoder);

        std::size_t clcl_size = 0;
        std::size_t nr_bits_left = dh_decoder.decode_nr_code_len_codes(clcl_size);
        if (nr_bits_left)
            return "failed decoding number of code lengths codes";

        std::array<CodeLenCodeLen, DHuffman::code_len_alphabet_size> clcl {};
        auto clcl_it = clcl.begin(); auto clcl_it_end = clcl_it + clcl_size;

        clcl_it = dh_decoder.decode_code_len_code_lens(clcl_it, clcl_it_end);
        if (clcl_it != clcl_it_end)
            return "failed decoding code lengths for the code length alphabet";
        else if (not DHuffman::validate_code_len_code_lens(clcl.begin(), clcl_it_end))
            return "invalid code lengths for the code length alphabet decoded";

        std::array<CodeLenCode, DHuffman::code_len_alphabet_size> clc {};
        DHuffman::gen_code_len_codes(clcl.begin(), clcl.begin() + clcl_size, clc.begin());
        HuffmanTree tree;
        tree.build_from_codes(clc.begin(),  clc.begin() + clcl_size,
                             clcl.begin(), clcl.begin() + clcl_size);

        cl_it = dh_decoder.decode_code_len_syms(tree.get_root(), cl_it, cl_it_end);
        if (cl_it != cl_it_end)
            return "failed decoding code lengths";
        else
            return success;
    }

    template<bool expect_term, std::output_iterator<char> OutIt>
    std::tuple<OutIt, Error<Decoder>> decode_data(OutIt out_it, OutIt out_it_end)
    {
        std::array<CodeLen, alphabet_size> code_lens {};
        auto e = decode_code_lens(code_lens.begin(), code_lens.end());
        if (e)
            return {out_it, {"failed decoding code lengths", e.report()}};
        else if (not Huffman<Policy>::validate_code_lens(code_lens.begin(), code_lens.end()))
            return {out_it, "invalid code lengths decoded"};

        std::array<Code, alphabet_size> codes {};
        Huffman<Policy>::gen_codes(code_lens.begin(), code_lens.end(), codes.data());

        HuffmanTree tree;
        auto ee = tree.build_from_codes(codes.begin(), codes.end(), code_lens.begin(), code_lens.end());
        if (ee)
            return {out_it, "failed building a Huffman tree"};

        auto huffman_decoder = Huffman<Policy>::make_decoder(bit_decoder);
        if constexpr (expect_term)
            out_it = huffman_decoder.decode_codes(tree.get_root(), out_it, out_it_end, term_sym);
        else
            out_it = huffman_decoder.decode_codes(tree.get_root(), out_it, out_it_end);
        return {out_it, success};
    }

private:
    BitDecoder& bit_decoder;
};

template<bool use_term = true, typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>,
        std::input_iterator InIt = typename std::vector<char>::iterator,
        std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator,
        typename ...OutItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) &&
              (Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7) &&
              sizeof...(OutItEnd) <= 1)
inline std::tuple<InIt, Error<>> huffman15_encode(
        misc::BitEncoder<Char, char_size, OutIt, OutItEnd...>& bit_encoder, InIt in_it, InIt in_it_end)
{
    return Huffman15<Policy, CodeLenPolicy>::make_encoder(bit_encoder).
        template encode_data<use_term>(in_it, in_it_end);
}

template<bool expect_term = true, typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>,
        std::output_iterator<char> OutIt = typename std::vector<Char>::iterator,
        std::input_iterator InIt = typename std::vector<Char>::iterator,
        typename ...InItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) &&
              (Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7) &&
              sizeof...(InItEnd) <= 1)
inline std::tuple<OutIt, Error<>> huffman15_decode(
        OutIt out_it, OutIt out_it_end, misc::BitDecoder<Char, char_size, InIt, InItEnd...>& bit_decoder)
{
    return Huffman15<Policy, CodeLenPolicy>::make_decoder(bit_decoder).
        template decode_data<expect_term>(out_it, out_it_end);
}

template<bool use_term = true, typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>,
        std::input_iterator InIt = typename std::vector<char>::iterator,
        std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator,
        typename ...OutItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) &&
              (Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7) &&
              sizeof...(OutItEnd) <= 1)
std::tuple<InIt, OutIt, Error<>> huffman15_encode(
        InIt in_it, InIt in_it_end, OutIt out_it, OutItEnd... out_it_end)
{
    auto bit_encoder = misc::make_bit_encoder<Char, char_size>(out_it, out_it_end...);

    Error<> e;
    std::tie(in_it, e) = huffman15_encode<use_term>(bit_encoder, in_it, in_it_end);
    out_it = bit_encoder.get_it();
    if (e)
        return std::make_tuple(in_it, out_it, e);

    bit_encoder.finalize();
    out_it = bit_encoder.get_it();
    return std::make_tuple(in_it, out_it, success);
}

template<bool expect_term = true, typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>,
        std::output_iterator<char> OutIt = typename std::vector<Char>::iterator,
        std::input_iterator InIt = typename std::vector<Char>::iterator,
        typename ...InItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) &&
              (Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7) &&
              sizeof...(InItEnd) <= 1)
std::tuple<OutIt, InIt, Error<>> huffman15_decode(
        OutIt out_it, OutIt out_it_end, InIt in_it, InItEnd... in_it_end)
{
    auto bit_decoder = misc::make_bit_decoder<Char, char_size>(in_it, in_it_end...);
    Error<> e;
    std::tie(out_it, e) = huffman15_decode<expect_term>(out_it, out_it_end, bit_decoder);
    in_it = bit_decoder.get_it();
    return std::make_tuple(out_it, in_it, e);
}

}