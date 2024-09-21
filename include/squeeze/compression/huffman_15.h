#pragma once

#include <array>
#include <tuple>

#include "huffman.h"
#include "deflate_huffman.h"
#include "squeeze/status.h"

namespace squeeze::compression {

/* A special Huffman coding interface meant to provide methods for a complete encoding/decoding of
 * byte data using Huffman coding with 15 bit code length limit and DeflateHuffman for encoding/decoding
 * the code lengths with 7 bit code length limit. */
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

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator, typename OutItEnd = void>
    class Encoder;

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::input_iterator InIt = typename std::vector<Char>::iterator, typename InItEnd = void>
    class Decoder;

    /* The alphabet size. All possible 256 bytes + one terminator symbol. */
    static constexpr std::size_t alphabet_size = 257;
    /* The terminator symbol. */
    static constexpr int term_sym = 0x100;

public:
    /* Make an encoder using the given bit encoder. */
    template<typename Char, std::size_t char_size, std::output_iterator<Char> OutIt, typename OutItEnd>
    inline static auto make_encoder(misc::BitEncoder<Char, char_size, OutIt, OutItEnd>& bit_encoder)
    {
        return Encoder<Char, char_size, OutIt, OutItEnd>(bit_encoder);
    }

    /* Make a decoder using the given bit decoder. */
    template<typename Char, std::size_t char_size, std::input_iterator InIt, typename InItEnd>
    inline static auto make_decoder(misc::BitDecoder<Char, char_size, InIt, InItEnd>& bit_decoder)
    {
        return Decoder<Char, char_size, InIt, InItEnd>(bit_decoder);
    }

    /* Utility for counting symbol frequencies. */
    template<std::input_iterator InIt, RandomAccessOutputIterator<Freq> FreqIt>
    static void count_freqs(InIt in_it, InIt in_it_end, FreqIt freq_it)
        requires std::convertible_to<std::iter_value_t<InIt>, char>
    {
        for (; in_it != in_it_end; ++in_it)
            ++freq_it[static_cast<unsigned char>(*in_it)];
    }
};

/* Huffman15::Encoder class. Provides methods for encoding code lengths as well as complete data encoding. */
template<HuffmanPolicy Policy, HuffmanPolicy CodeLenPolicy>
    requires (Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7)
template<typename Char, std::size_t char_size, std::output_iterator<Char> OutIt, typename OutItEnd>
class Huffman15<Policy, CodeLenPolicy>::Encoder {
public:
    using Stat = StatStr;
    using BitEncoder = misc::BitEncoder<Char, char_size, OutIt, OutItEnd>;

    explicit Encoder(BitEncoder& bit_encoder) : bit_encoder(bit_encoder)
    {
    }

    template<std::forward_iterator CodeLenIt>
    inline Stat encode_code_lens(CodeLenIt cl_it, CodeLenIt cl_it_end)
    {
        return DHuffman::make_encoder(bit_encoder).encode_code_lens(cl_it,  cl_it_end);
    }

    /* Encode data. Generates and encodes the code lengths and the main data. */
    template<bool use_term, std::forward_iterator InIt>
    std::tuple<InIt, Stat> encode_data(InIt in_it, InIt in_it_end)
        requires std::convertible_to<std::iter_value_t<InIt>, char>
    {
        std::array<Freq, alphabet_size> freqs {};
        freqs[term_sym] = use_term;

        count_freqs(in_it, in_it_end, freqs.data());
        std::array<CodeLen, alphabet_size> code_lens {};
        Huffman<Policy>::sort_find_code_lengths(freqs.begin(), freqs.end(), code_lens.begin());
        std::array<Code, alphabet_size> codes {};
        Huffman<Policy>::gen_codes(code_lens.begin(), code_lens.end(), codes.data());

        Stat s = encode_code_lens(code_lens.begin(), code_lens.end());
        if (s.failed()) [[unlikely]]
            return std::make_tuple(in_it, Stat{"failed encoding code lengths", s});

        auto huffman_encoder = Huffman<Policy>::make_encoder(bit_encoder);
        if constexpr (use_term)
            in_it = huffman_encoder.encode_syms(codes.data(), code_lens.data(), in_it, in_it_end, term_sym);
        else
            in_it = huffman_encoder.encode_syms(codes.data(), code_lens.data(), in_it, in_it_end);
        return {in_it, success};
    }

private:
    BitEncoder& bit_encoder;
};

/* Huffman15::Decoder class. Provides methods for decoding code lengths as well as complete data decoding. */
template<HuffmanPolicy Policy, HuffmanPolicy CodeLenPolicy>
    requires (Policy::code_len_limit == 15 and CodeLenPolicy::code_len_limit == 7)
template<typename Char, std::size_t char_size, std::input_iterator InIt, typename InItEnd>
class Huffman15<Policy, CodeLenPolicy>::Decoder {
public:
    using Stat = StatStr;
    using BitDecoder = misc::BitDecoder<Char, char_size, InIt, InItEnd>;

    explicit Decoder(BitDecoder& bit_decoder) : bit_decoder(bit_decoder)
    {
    }

    template<std::output_iterator<CodeLen> CodeLenIt>
    inline Stat decode_code_lens(CodeLenIt cl_it, CodeLenIt cl_it_end)
    {
        return DHuffman::make_decoder(bit_decoder).decode_code_lens(cl_it, cl_it_end);
    }

    /* Decode data. Decodes the code lengths and the main data. */
    template<bool expect_term, std::output_iterator<char> OutIt>
    std::tuple<OutIt, Stat> decode_data(OutIt out_it, OutIt out_it_end)
    {
        std::array<CodeLen, alphabet_size> code_lens {};
        Stat s = decode_code_lens(code_lens.begin(), code_lens.end());
        if (s.failed())
            return {out_it, Stat{"failed decoding code lengths", s}};
        else if (not Huffman<Policy>::validate_code_lens(code_lens.begin(), code_lens.end()))
            return {out_it, "invalid code lengths decoded"};

        std::array<Code, alphabet_size> codes {};
        Huffman<Policy>::gen_codes(code_lens.begin(), code_lens.end(), codes.data());

        HuffmanTree tree;
        StatStr st = tree.build_from_codes(codes.begin(), codes.end(), code_lens.begin(), code_lens.end());
        if (st.failed())
            return {out_it, Stat{"failed building a Huffman tree", st}};

        auto huffman_decoder = Huffman<Policy>::make_decoder(bit_decoder);
        if constexpr (expect_term)
            out_it = huffman_decoder.decode_syms(tree.get_root(), out_it, out_it_end, term_sym);
        else
            out_it = huffman_decoder.decode_syms(tree.get_root(), out_it, out_it_end);
        return {out_it, success};
    }

private:
    BitDecoder& bit_decoder;
};

/* Encode data using Huffman15 with the given bit encoder.
 * Return input iterator at the point when encoding stopped and an error or success message. */
template<bool use_term = true, typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>,
        std::input_iterator InIt = typename std::vector<char>::iterator,
        std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator,
        typename ...OutItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) &&
              (Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7) &&
              sizeof...(OutItEnd) <= 1)
inline std::tuple<InIt, StatStr> huffman15_encode(
        misc::BitEncoder<Char, char_size, OutIt, OutItEnd...>& bit_encoder, InIt in_it, InIt in_it_end)
{
    return Huffman15<Policy, CodeLenPolicy>::make_encoder(bit_encoder).
        template encode_data<use_term>(in_it, in_it_end);
}

/* Decode data using Huffman15 with the given bit encoder.
 * Return output iterator at the point when decoding stopped and an error or success message. */
template<bool expect_term = true, typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>,
        std::output_iterator<char> OutIt = typename std::vector<Char>::iterator,
        std::input_iterator InIt = typename std::vector<Char>::iterator,
        typename ...InItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) &&
              (Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7) &&
              sizeof...(InItEnd) <= 1)
inline std::tuple<OutIt, StatStr> huffman15_decode(
        OutIt out_it, OutIt out_it_end, misc::BitDecoder<Char, char_size, InIt, InItEnd...>& bit_decoder)
{
    return Huffman15<Policy, CodeLenPolicy>::make_decoder(bit_decoder).
        template decode_data<expect_term>(out_it, out_it_end);
}

/* Encode data using Huffman15. Return (InIt, OutIt, Error) triple at the point when encoding stopped. */
template<bool use_term = true, typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>,
        std::input_iterator InIt = typename std::vector<char>::iterator,
        std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator,
        typename ...OutItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) &&
              (Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7) &&
              sizeof...(OutItEnd) <= 1)
std::tuple<InIt, OutIt, StatStr> huffman15_encode(
        InIt in_it, InIt in_it_end, OutIt out_it, OutItEnd... out_it_end)
{
    auto bit_encoder = misc::make_bit_encoder<Char, char_size>(out_it, out_it_end...);

    StatStr s = success;
    std::tie(in_it, s) = huffman15_encode<use_term>(bit_encoder, in_it, in_it_end);
    out_it = bit_encoder.get_it();
    if (s.failed())
        return std::make_tuple(in_it, out_it, std::move(s));

    bit_encoder.finalize();
    out_it = bit_encoder.get_it();
    return std::make_tuple(in_it, out_it, success);
}

/* Decode data using Huffman15. Return (OutIt, InIt, Error) triple at the point when decoding stopped. */
template<bool expect_term = true, typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        HuffmanPolicy Policy = BasicHuffmanPolicy<15>, HuffmanPolicy CodeLenPolicy = BasicHuffmanPolicy<7>,
        std::output_iterator<char> OutIt = typename std::vector<char>::iterator,
        std::input_iterator InIt = typename std::vector<Char>::iterator,
        typename ...InItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) &&
              (Policy::code_len_limit == 15 && CodeLenPolicy::code_len_limit == 7) &&
              sizeof...(InItEnd) <= 1)
std::tuple<OutIt, InIt, StatStr> huffman15_decode(
        OutIt out_it, OutIt out_it_end, InIt in_it, InItEnd... in_it_end)
{
    auto bit_decoder = misc::make_bit_decoder<Char, char_size>(in_it, in_it_end...);
    StatStr s = success;
    std::tie(out_it, s) = huffman15_decode<expect_term>(out_it, out_it_end, bit_decoder);
    in_it = bit_decoder.get_it();
    return std::make_tuple(out_it, in_it, std::move(s));
}

}
