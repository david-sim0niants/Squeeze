#pragma once

#include <array>

#include "huffman.h"
#include "deflate_huffman.h"
#include "error.h"

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

    static constexpr std::size_t alphabet_size = 256;

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
    EncodeError<Encoder> encode_code_lens(CodeLenIt cl_it, CodeLenIt cl_it_end)
    {
        std::array<CodeLenCodeLen, DHuffman::code_len_alphabet_size> clcl {};
        DHuffman::gen_code_len_code_lens(cl_it, cl_it_end, clcl.begin());
        std::array<CodeLenCode, DHuffman::code_len_alphabet_size> clc {};
        DHuffman::gen_code_len_codes(clcl.begin(), clcl.end(), clc.begin());

        std::size_t clcl_size = clcl.size();
        for (; clcl_size > DHuffman::min_nr_code_len_codes && clcl[clcl_size - 1] == 0; --clcl_size);

        auto dh_encoder = DHuffman::make_encoder(bit_encoder);
        std::size_t nr_bits_left = 0;

        nr_bits_left = dh_encoder.encode_nr_code_len_codes(clcl_size);
        if (nr_bits_left)
            return {{"failed encoding number of code length codes", nr_bits_left}};

        nr_bits_left = dh_encoder.encode_code_len_code_lens(clcl.begin(), clcl.begin() + clcl_size);
        if (nr_bits_left)
            return {{"failed encoding code lengths for the code length alphabet", nr_bits_left}};

        nr_bits_left = dh_encoder.encode_code_len_syms(clc.begin(), clcl.begin(), cl_it, cl_it_end);
        if (nr_bits_left)
            return {{"failed encoding code lengths", nr_bits_left}};

        return success;
    }

    template<std::forward_iterator InIt>
    EncodeError<Encoder> encode_data(InIt in_it, InIt in_it_end)
        requires std::convertible_to<std::iter_value_t<InIt>, char>
    {
        std::array<Freq, alphabet_size> freqs {};
        count_freqs(in_it, in_it_end, freqs.data());
        std::array<CodeLen, alphabet_size> code_lens {};
        Huffman<Policy>::sort_find_code_lengths(freqs.begin(), freqs.end(), code_lens.begin());
        std::array<Code, alphabet_size> codes {};
        Huffman<Policy>::gen_codes(code_lens.begin(), code_lens.end(), codes.data());

        auto e = encode_code_lens(code_lens.begin(), code_lens.end());
        if (e)
            return e;
        auto huffman_encoder = Huffman<Policy>::make_encoder(bit_encoder);
        auto ee = huffman_encoder.encode_codes(codes.begin(), code_lens.begin(), in_it, in_it_end);
        if (ee)
            return ee;
        return success;
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
    DecodeError<Decoder> decode_code_lens(CodeLenIt cl_it, CodeLenIt cl_it_end)
    {
        auto dh_decoder = DHuffman::make_decoder(bit_decoder);
        std::size_t nr_bits_left = 0;

        std::size_t clcl_size = 0;
        nr_bits_left = dh_decoder.decode_nr_code_len_codes(clcl_size);
        if (nr_bits_left)
            return {{"failed decoding number of code lengths codes", nr_bits_left}};

        std::array<CodeLenCodeLen, DHuffman::code_len_alphabet_size> clcl {};
        nr_bits_left = dh_decoder.decode_code_len_code_lens(clcl.begin(), clcl.begin() + clcl_size);
        if (nr_bits_left)
            return {{"failed decoding code lengths for the code length alphabet"}};

        std::array<CodeLenCode, DHuffman::code_len_alphabet_size> clc {};
        DHuffman::gen_code_len_codes(clcl.begin(), clcl.begin() + clcl_size, clc.begin());
        HuffmanTree tree;
        tree.build_from_codes(clc.begin(), clc.begin() + clcl_size,
                clcl.begin(), clcl.begin() + clcl_size);

        nr_bits_left = dh_decoder.decode_code_len_syms(tree.get_root(), cl_it, cl_it_end);
        if (nr_bits_left)
            return {{"failed decoding code lengths", nr_bits_left}};

        return success;
    }

    template<std::output_iterator<char> OutIt>
    DecodeError<Decoder> decode_data(OutIt out_it, OutIt out_it_end)
    {
        std::array<CodeLen, alphabet_size> code_lens {};
        auto e = decode_code_lens(code_lens.begin(), code_lens.end());
        if (e)
            return e;

        std::array<Code, alphabet_size> codes {};
        Huffman<Policy>::gen_codes(code_lens.begin(), code_lens.end(), codes.data());

        auto huffman_decoder = Huffman<Policy>::make_decoder(bit_decoder);
        HuffmanTree tree;
        auto ee = tree.build_from_codes(codes.begin(), codes.end(), code_lens.begin(), code_lens.end());
        if (ee)
            return {{"failed building a Huffman tree", 0}};

        auto eee = huffman_decoder.decode_codes(tree.get_root(), out_it, out_it_end);
        if (eee)
            return eee;

        return success;
    }

private:
    BitDecoder& bit_decoder;
};

template<HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>,
        typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator,
        std::input_iterator InIt = typename std::vector<char>::iterator>
    requires ((Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7) &&
              (sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT))
EncodeError<> huffman15_encode(InIt in_it, InIt in_it_end, OutIt out_it)
{
    auto bit_encoder = misc::make_bit_encoder(out_it);
    auto huffman_encoder = Huffman15<Policy, CodeLenPolicy>::make_encoder(bit_encoder);
    auto e = huffman_encoder.encode_data(in_it, in_it_end);
    if (e)
        return e;
    std::size_t nr_bits_left = bit_encoder.finalize();
    if (nr_bits_left) [[unlikely]]
        return {{"failed finalizing bit encoding", nr_bits_left}};
    return success;
}

template<HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>,
        typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator,
        std::input_iterator InIt = typename std::vector<char>::iterator>
    requires ((Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7) &&
              (sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT))
EncodeError<> huffman15_encode(InIt in_it, InIt in_it_end, OutIt out_it, OutIt out_it_end)
{
    auto bit_encoder = misc::make_bit_encoder(out_it, out_it_end);
    auto huffman_encoder = Huffman15<Policy, CodeLenPolicy>::make_encoder(bit_encoder);
    auto e = huffman_encoder.encode_data(in_it, in_it_end);
    if (e)
        return e;
    std::size_t nr_bits_left = bit_encoder.finalize();
    if (nr_bits_left) [[unlikely]]
        return {{"failed finalizing bit encoding", nr_bits_left}};
    return success;
}

template<HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>,
        typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::output_iterator<Char> InIt = typename std::vector<Char>::iterator,
        std::input_iterator OutIt = typename std::vector<char>::iterator>
    requires ((Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7) &&
              (sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT))
DecodeError<> huffman15_decode(OutIt out_it, OutIt out_it_end, InIt in_it)
{
    auto bit_decoder = misc::make_bit_decoder(in_it);
    auto huffman_decoder = Huffman15<Policy, CodeLenPolicy>::make_decoder(bit_decoder);
    auto e = huffman_decoder.decode_data(out_it, out_it_end);
    if (e)
        return e;
    return success;
}

template<HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>,
        typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::output_iterator<Char> InIt = typename std::vector<Char>::iterator,
        std::input_iterator OutIt = typename std::vector<char>::iterator>
    requires ((Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7) &&
              (sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT))
DecodeError<> huffman15_decode(OutIt out_it, OutIt out_it_end, InIt in_it, InIt in_it_end)
{
    auto bit_decoder = misc::make_bit_decoder(in_it, in_it_end);
    auto huffman_decoder = Huffman15<Policy, CodeLenPolicy>::make_decoder(bit_decoder);
    auto e = huffman_decoder.decode_data(out_it, out_it_end);
    if (e)
        return e;
    return success;
}

}
