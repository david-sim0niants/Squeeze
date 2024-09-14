#pragma once

#include "deflate_huffman.h"
#include "deflate_lz77.h"
#include "deflate_policy.h"
#include "deflate_params.h"

#include <span>

namespace squeeze::compression {

template<DeflatePolicy Policy = BasicDeflatePolicy>
class Deflate {
public:
    using HuffmanPolicy = typename Policy::HuffmanPolicy;
    using CodeLenHuffmanPolicy = typename Policy::CodeLenHuffmanPolicy;

    using Huffman_ = Huffman<HuffmanPolicy>;
    using DHuffman = DeflateHuffman<HuffmanPolicy, CodeLenHuffmanPolicy>;

    using Freq = typename Huffman_::Freq;
    using Code = typename Huffman_::Code;
    using CodeLen = typename Huffman_::CodeLen;

    using CodeLenCode = typename DHuffman::CodeLenCode;
    using CodeLenCodeLen = typename DHuffman::CodeLenCodeLen;

    using Literal = DeflateLZ77::Literal;
    using LitLenSym = uint16_t;
    using LenSym = typename DeflateLZ77::LenSym;
    using DistSym = typename DeflateLZ77::DistSym;

    using PackedToken = DeflateLZ77::PackedToken;

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
             std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator, typename ...OutItEnd>
        requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(OutItEnd) <= 1)
    class Encoder;

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
             std::output_iterator<char> OutIt = typename std::vector<Char>::iterator,
             std::input_iterator InIt = typename std::vector<Char>::iterator, typename ...InItEnd>
        requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(InItEnd) <= 1)
    class Decoder;

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

    static constexpr std::size_t literal_alphabet_size = 257;
    static constexpr std::size_t len_alphabet_size = DeflateLZ77::max_len_sym + 1;
    static constexpr std::size_t litlen_alphabet_size = literal_alphabet_size + len_alphabet_size;
    static constexpr std::size_t dist_alphabet_size = DeflateLZ77::max_dist_sym + 1;
    static constexpr LitLenSym term_sym = 0x100;
};

template<DeflatePolicy Policy>
template<typename Char, std::size_t char_size,
         std::output_iterator<Char> OutIt, typename ...OutItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(OutItEnd) <= 1)
class Deflate<Policy>::Encoder {
public:
    using BitEncoder = misc::BitEncoder<Char, char_size, OutIt, OutItEnd...>;
private:
    using IntermediateData = std::vector<PackedToken>;

public:
    explicit Encoder(BitEncoder& bit_encoder) : bit_encoder(bit_encoder)
    {
    }

    template<bool final_block, std::input_iterator InIt, typename... InItEnd>
        requires (sizeof...(InItEnd) <= 1)
    Error<Encoder> encode(const DeflateParams& params, InIt in_it, InItEnd... in_it_end)
    {
        return huffman_encode<final_block>(lz77_encode(params.lz77_encoder_params, in_it, in_it_end...));
    }

private:
    template<std::input_iterator InIt, typename... InItEnd>
        requires (sizeof...(InItEnd) <= 1)
    IntermediateData lz77_encode(const LZ77EncoderParams& params, InIt in_it, InItEnd... in_it_end)
    {
        auto lz77_encoder = DeflateLZ77::make_encoder(params, in_it, in_it_end...);
        IntermediateData lz77_output;
        PackedToken extra_token = lz77_encoder.encode(std::back_inserter(lz77_output));
        assert(extra_token.is_none());
        return lz77_output;
    }

    template<bool final_block>
    Error<Encoder> huffman_encode(const IntermediateData& data)
    {
        std::array<CodeLen, litlen_alphabet_size + dist_alphabet_size> code_lens_storage {};
        auto [litlen_code_lens, dist_code_lens] = find_code_lens(data, code_lens_storage);

        std::span both_code_lens {code_lens_storage.data(), litlen_code_lens.size() + dist_code_lens.size()};

        Error<Encoder> e = success;
        if ((e = huffman_encode_code_lens(litlen_code_lens, dist_code_lens, both_code_lens)).failed())
            return {"failed encoding code lengths", e.report()};
        if ((e = huffman_encode_syms<final_block>(data, litlen_code_lens, dist_code_lens)))
            return {"failed encoding data", e.report()};
        return success;
    }

    Error<Encoder> huffman_encode_code_lens(std::span<CodeLen> litlen_code_lens,
            std::span<CodeLen> dist_code_lens,
            std::span<CodeLen> both_code_lens)
    {
        const std::size_t nr_litlen_codes = litlen_code_lens.size();
        const std::size_t nr_dist_codes = dist_code_lens.size();

        std::array<CodeLenCodeLen, DHuffman::code_len_alphabet_size> clcl_storage {};
        auto clcl = find_code_len_code_lens(both_code_lens, clcl_storage);

        Error<> e = success;
        if ((e = huffman_encode_nr_codes(nr_litlen_codes, nr_dist_codes, clcl.size())).successful() &&
            (e = huffman_encode_code_len_code_lens(clcl)).successful()                              &&
            (e = huffman_encode_code_len_syms(clcl, both_code_lens)).successful())
            return success;
        else
            return e;
    }

    Error<Encoder> huffman_encode_nr_codes(
            std::size_t nr_litlen_codes,
            std::size_t nr_dist_codes,
            std::size_t nr_code_len_codes)
    {
        assert(nr_litlen_codes - literal_alphabet_size <= 31);
        assert(nr_dist_codes - 1 <= 31);

        if (bit_encoder.encode_bits(std::bitset<5>(nr_litlen_codes - literal_alphabet_size)) ||
            bit_encoder.encode_bits(std::bitset<5>(nr_dist_codes - 1))                       ||
            DHuffman::make_encoder(bit_encoder).encode_nr_code_len_codes(nr_code_len_codes))
            return "failed encoding the (HLIT, HDIST, HCLEN) triple of the number of code lengths";
        else
            return success;
    }

    Error<Encoder> huffman_encode_code_len_code_lens(std::span<CodeLenCodeLen> clcl)
    {
        auto dh_encoder = DeflateHuffman<>::make_encoder(bit_encoder);
        auto clcl_it = dh_encoder.encode_code_len_code_lens(clcl.begin(), clcl.end());
        if (clcl_it != clcl.end()) [[unlikely]]
            return "failed encoding code lengths for the code length alphabet";
        else
            return success;
    }

    Error<Encoder> huffman_encode_code_len_syms(std::span<CodeLenCodeLen> clcl, std::span<CodeLen> cl)
    {
        std::array<CodeLenCode, DHuffman::code_len_alphabet_size> clc {};
        DHuffman::gen_code_len_codes(clcl.begin(), clcl.end(), clc.data());

        auto dh_encoder = DeflateHuffman<>::make_encoder(bit_encoder);
        auto cl_it = dh_encoder.encode_code_len_syms(clc.data(), clcl.data(), cl.begin(), cl.end());
        if (cl_it != cl.end()) [[unlikely]]
            return "failed encoding code lengths";
        else
            return success;
    }

    template<bool final_block>
    Error<Encoder> huffman_encode_syms(const IntermediateData& data,
            std::span<CodeLen> litlen_code_lens, std::span<CodeLen> dist_code_lens)
    {
        std::array<Code, litlen_alphabet_size> litlen_codes_storage;
        std::array<Code, dist_alphabet_size> dist_codes_storage;
        auto [litlen_codes, dist_codes] = gen_codes(litlen_code_lens, dist_code_lens,
                litlen_codes_storage, dist_codes_storage);

        for (auto it = data.begin(); it != data.end(); ++it) {
            PackedToken token = *it;
            if (token.is_literal()) {
                LitLenSym sym = static_cast<std::make_unsigned_t<Literal>>(token.get_literal());
                bit_encoder.encode_bits(litlen_codes[sym], litlen_code_lens[sym]);
            } else if (token.is_len_dist()) {
                ++it;
                if (it == data.end())
                    throw Exception<Encoder>("expected a distance extra bits token");

                PackedToken extra = *it;

                LitLenSym len_sym = static_cast<std::make_unsigned_t<LenSym>>(token.get_len_sym());
                DistSym dist_sym = static_cast<std::make_unsigned_t<DistSym>>(token.get_dist_sym());

                // TODO: missed the symbol to extra bits length conversion part, DO IT

            } else [[unlikely]] {
                throw Exception<Encoder>("invalid token");
            }
        }
        return "not ready";
    }

    std::tuple<std::span<CodeLen>, std::span<CodeLen>> find_code_lens(
            const IntermediateData& data, std::span<CodeLen> code_lens)
    {
        std::array<Freq, litlen_alphabet_size> litlen_freq {};
        std::array<Freq, dist_alphabet_size> dist_freq {};
        count_freqs(data, litlen_freq, dist_freq);

        std::span<CodeLen> litlen_code_lens (code_lens.data(), litlen_alphabet_size);
        Huffman_::sort_find_code_lengths(litlen_freq.begin(), litlen_freq.end(), litlen_code_lens.data());
        assert(Huffman_::validate_code_lens(litlen_code_lens.begin(), litlen_code_lens.end()));
        litlen_code_lens = strip_trailing_zeros(litlen_code_lens, literal_alphabet_size);

        std::span<CodeLen> dist_code_lens (code_lens.data() + litlen_code_lens.size(), dist_alphabet_size);
        Huffman_::sort_find_code_lengths(dist_freq.begin(), dist_freq.end(), dist_code_lens.data());
        assert(Huffman_::validate_code_lens(dist_code_lens.begin(), dist_code_lens.end()));
        dist_code_lens = strip_trailing_zeros(dist_code_lens, 1);

        return std::make_tuple(litlen_code_lens, dist_code_lens);
    }

    std::span<CodeLenCodeLen> find_code_len_code_lens(
            std::span<CodeLen> code_lens, std::span<CodeLenCodeLen> clcl)
    {
        DHuffman::find_code_len_code_lens(code_lens.begin(), code_lens.end(), clcl.data());
        assert(DHuffman::validate_code_len_code_lens(clcl.begin(), clcl.end()));
        return strip_trailing_zeros(clcl, DHuffman::min_nr_code_len_codes);
    }

    std::tuple<std::span<Code>, std::span<Code>> gen_codes(
            std::span<CodeLen> litlen_code_lens, std::span<CodeLen> dist_code_lens,
            std::span<Code> litlen_codes_storage, std::span<Code> dist_codes_storage)
    {
        std::span litlen_codes (litlen_codes_storage.data(), litlen_code_lens.size());
        std::span dist_codes (dist_codes_storage.data(), dist_code_lens.size());

        Huffman_::gen_codes(litlen_code_lens.begin(), litlen_code_lens.end(), litlen_codes.data());
        Huffman_::gen_codes(dist_code_lens.begin(), dist_code_lens.end(), dist_codes.data());

        return std::make_tuple(litlen_codes, dist_codes);
    }

    template<std::integral T>
    static std::span<T> strip_trailing_zeros(std::span<T> code_lens, std::size_t min_size)
    {
        std::size_t size = code_lens.size();
        for (; size > min_size && code_lens[size - 1] == 0; --size);
        return code_lens.first(size);
    }

    static void count_freqs(const IntermediateData& data,
            std::array<Freq, litlen_alphabet_size>& litlen_freq,
            std::array<Freq, dist_alphabet_size>& dist_freq)
    {
        for (std::size_t i = 0; i < data.size(); ++i) {
            DeflateLZ77::PackedToken token = data[i];
            if (token.is_literal()) {
                ++litlen_freq[static_cast<std::make_unsigned_t<Literal>>(token.get_literal())];
            } else if (token.is_len_dist()) {
                const LenSym len_sym = token.get_len_sym();
                const LitLenSym lit_len_sym = static_cast<LitLenSym>(len_sym) + literal_alphabet_size;
                const DistSym dist_sym = token.get_dist_sym();

                assert(lit_len_sym < litlen_alphabet_size);
                assert(dist_sym < dist_alphabet_size);

                ++litlen_freq[lit_len_sym];
                ++dist_freq[dist_sym];
                ++i;

                if (i == data.size()) [[unlikely]]
                    throw Exception<Encoder>("expected a distance extra bits token");
            } else [[unlikely]] {
                throw Exception<Encoder>("invalid token");
            }
        }
        ++litlen_freq[term_sym];
    }

    BitEncoder& bit_encoder;
};

template<bool final_block = false,
         typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
         DeflatePolicy Policy = BasicDeflatePolicy,
         std::input_iterator InIt = typename std::vector<Char>::iterator,
         std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator, typename ...OutItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(OutItEnd) <= 1)
inline std::tuple<InIt, Error<>> deflate(misc::BitEncoder<char, CHAR_BIT, OutIt, OutItEnd...>& bit_encoder,
        InIt in_it, InIt in_it_end, const DeflateParams& params)
{
    auto encoder = Deflate<Policy>::make_encoder(bit_encoder);
    return std::make_tuple(in_it_end, encoder.template encode<final_block>(params, in_it, in_it_end));
}

template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
         DeflatePolicy Policy = BasicDeflatePolicy,
         std::output_iterator<char> OutIt = typename std::vector<Char>::iterator,
         std::input_iterator InIt = typename std::vector<Char>::iterator, typename ...InItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(InItEnd) <= 1)
std::tuple<OutIt, Error<>> inflate(OutIt out_it, OutIt out_it_end,
        misc::BitDecoder<Char, char_size, InIt, InItEnd...>& bit_decoder)
{
    throw BaseException(__PRETTY_FUNCTION__ + std::string(" is unimplemented"));
}

template<bool final_block = false,
         typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
         DeflatePolicy Policy = BasicDeflatePolicy,
         std::input_iterator InIt = typename std::vector<Char>::iterator,
         std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator, typename ...OutItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(OutItEnd) <= 1)
std::tuple<InIt, OutIt, Error<>> deflate(const DeflateParams& deflate_params,
        InIt in_it, InIt in_it_end, OutIt out_it, OutItEnd... out_it_end)
{
    auto bit_encoder = misc::make_bit_encoder<Char, char_size>(out_it, out_it_end...);

    Error<> e;
    std::tie(in_it, e) = deflate<final_block>(bit_encoder, in_it, in_it_end, deflate_params);
    out_it = bit_encoder.get_it();
    if (e)
        return std::make_tuple(in_it, out_it, e);

    bit_encoder.finalize();
    out_it = bit_encoder.get_it();
    return std::make_tuple(in_it, out_it, success);
}

template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
         DeflatePolicy Policy = BasicDeflatePolicy,
         std::output_iterator<char> OutIt = typename std::vector<Char>::iterator,
         std::input_iterator InIt = typename std::vector<Char>::iterator, typename ...InItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(InItEnd) <= 1)
std::tuple<OutIt, InIt, Error<>> inflate(OutIt out_it, OutIt out_it_end, InIt in_it, InItEnd... in_it_end)
{
    auto bit_decoder = misc::make_bit_decoder<Char, char_size>(in_it, in_it_end...);
    Error<> e;
    std::tie(out_it, e) = inflate(out_it, out_it_end, bit_decoder);
    in_it = bit_decoder.get_it();
    return std::make_tuple(out_it, in_it, e);
}

}
