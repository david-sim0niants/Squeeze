// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include "deflate_huffman.h"
#include "deflate_lz77.h"
#include "deflate_policy.h"
#include "deflate_params.h"

#include <span>

namespace squeeze::compression {

/** The Deflate coding interface. Provides methods for encoding/decoding data
 * using the DEFLATE algorithm as specified by the DEFLATE specification (RFC1951). */
template<DeflatePolicy Policy = BasicDeflatePolicy>
class Deflate {
public:
    /** Status return type of this class methods. */
    using Stat = StatStr;

    using HuffmanPolicy = typename Policy::HuffmanPolicy;
    using CodeLenHuffmanPolicy = typename Policy::CodeLenHuffmanPolicy;

    using Huffman_ = Huffman<HuffmanPolicy>;
    using DHuffman = DeflateHuffman<HuffmanPolicy, CodeLenHuffmanPolicy>;

    using Freq = typename Huffman_::Freq;
    using Code = typename Huffman_::Code;
    using CodeLen = typename Huffman_::CodeLen;

    using CodeLenCode = typename DHuffman::CodeLenCode;
    using CodeLenCodeLen = typename DHuffman::CodeLenCodeLen;

    /** Literal type used in an LZ77 intermediate representation. */
    using Literal = DeflateLZ77::Literal;
    /** Literal-Length symbol type. */
    using LitLenSym = uint16_t;
    /** Length symbol type used in an LZ77 intermediate representation. */
    using LenSym = DeflateLZ77::LenSym;
    /** Distance symbol type used in an LZ77 intermediate representation. */
    using DistSym = DeflateLZ77::DistSym;
    /** Length extra bits type used in an LZ77 intermediate representation. */
    using LenExtra = DeflateLZ77::LenExtra;
    /** Distance extra bits type used in an LZ77 intermediate representation. */
    using DistExtra = DeflateLZ77::DistExtra;

    /** Packed tokens used in an LZ77 intermediate representation. */
    using PackedToken = DeflateLZ77::PackedToken;

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
             std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator, typename ...OutItEnd>
        requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(OutItEnd) <= 1)
    class Encoder;

    template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
             std::input_iterator InIt = typename std::vector<Char>::iterator, typename ...InItEnd>
        requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(InItEnd) <= 1)
    class Decoder;

    /** Check if a literal/length symbol is a literal. */
    inline static constexpr bool is_literal(LitLenSym litlen_sym)
    {
        return litlen_sym < term_sym;
    }

    /** Get the literal/length symbol as a literal if it's one. */
    inline static constexpr Literal get_literal(LitLenSym litlen_sym)
    {
        return Literal(litlen_sym & 0xFF);
    }

    /** Check if the literal/length symbol is a terminator symbol. */
    inline static constexpr bool is_term(LitLenSym litlen_sym)
    {
        return litlen_sym == term_sym;
    }

    /** Check if the literal/length symbol is a length symbol. */
    inline static constexpr bool is_len_sym(LitLenSym litlen_sym)
    {
        return litlen_sym > term_sym && litlen_sym <= to_litlen(DeflateLZ77::max_len_sym);
    }

    /** Get the literal/length symbol as a length symbol if it's one. */
    inline static constexpr LenSym get_len_sym(LitLenSym litlen_sym)
    {
        return LenSym(litlen_sym - literal_term_alphabet_size);
    }

    /** Check if the distance symbol is valid. */
    inline static constexpr bool is_valid_dist_sym(DistSym dist_sym)
    {
        return dist_sym <= DeflateLZ77::max_dist_sym;
    }

    /** Convert the literal into a literal/length symbol. */
    inline static constexpr LitLenSym to_litlen(Literal literal)
    {
        return static_cast<std::make_unsigned_t<Literal>>(literal);
    }

    /** Convert the length symbol into a literal/length symbol. */
    inline static constexpr LitLenSym to_litlen(LenSym len_sym)
    {
        return static_cast<LitLenSym>(len_sym) + literal_term_alphabet_size;
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

    /** Size of alphabet of literals and the terminator symbol combined. */
    static constexpr std::size_t literal_term_alphabet_size = 257;
    /** Length alphabet size. */
    static constexpr std::size_t len_alphabet_size = DeflateLZ77::max_len_sym + 1;
    /** Literal/Length alphabet size. */
    static constexpr std::size_t litlen_alphabet_size = literal_term_alphabet_size + len_alphabet_size;
    /** Distance alphabet size. */
    static constexpr std::size_t dist_alphabet_size = DeflateLZ77::max_dist_sym + 1;
    /** The terminator symbol. */
    static constexpr LitLenSym term_sym = 0x100;

private:
    /** Generate literal/length and distance codes for the corresponding code lengths,
     * using the provided storages. */
    static std::tuple<std::span<Code>, std::span<Code>>
        gen_codes(std::span<const CodeLen> litlen_code_lens, std::span<const CodeLen> dist_code_lens,
                  std::span<Code> litlen_codes_storage, std::span<Code> dist_codes_storage)
    {
        std::span litlen_codes (litlen_codes_storage.data(), litlen_code_lens.size());
        std::span dist_codes (dist_codes_storage.data(), dist_code_lens.size());

        Huffman_::template gen_codes<litlen_alphabet_size>
            (litlen_code_lens.begin(), litlen_code_lens.end(), litlen_codes.data());
        Huffman_::template gen_codes<dist_alphabet_size>
            (dist_code_lens.begin(), dist_code_lens.end(), dist_codes.data());

        return std::make_tuple(litlen_codes, dist_codes);
    }

    /** Build literal/length and distance Huffman trees for the corresponding lengths. */
    static std::tuple<HuffmanTree, HuffmanTree, Stat> huffman_build_trees(
            std::span<const CodeLen> litlen_code_lens, std::span<const CodeLen> dist_code_lens)
    {
        std::array<Code, litlen_alphabet_size> litlen_codes_storage {};
        std::array<Code, dist_alphabet_size> dist_codes_storage {};
        auto [litlen_codes, dist_codes] = gen_codes(litlen_code_lens, dist_code_lens,
                litlen_codes_storage, dist_codes_storage);

        std::tuple<HuffmanTree, HuffmanTree, Stat> result;
        auto& [litlen_tree, dist_tree, s] = result;

        (s = litlen_tree.build_from_codes(litlen_codes.begin(), litlen_codes.end(),
                                          litlen_code_lens.begin(), litlen_code_lens.end())) &&
        (s = dist_tree.build_from_codes(dist_codes.begin(), dist_codes.end(),
                                        dist_code_lens.begin(), dist_code_lens.end()));

        assert(litlen_tree.get_root() == nullptr || litlen_tree.get_root()->validate_full_tree());
        assert(dist_tree.get_root() == nullptr || dist_tree.get_root()->validate_full_tree());

        return result;
    }
};

/** The Deflate::Encoder class. Supports encoding data, header_bits, and both at once. */
template<DeflatePolicy Policy>
template<typename Char, std::size_t char_size, std::output_iterator<Char> OutIt, typename ...OutItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(OutItEnd) <= 1)
class Deflate<Policy>::Encoder {
public:
    using Stat = StatStr;
    using BitEncoder = misc::BitEncoder<Char, char_size, OutIt, OutItEnd...>;

private:
    /** Intermediate LZ77 compressed data representation. */
    using IntermediateData = std::vector<PackedToken>;

public:
    explicit Encoder(BitEncoder& bit_encoder) : bit_encoder(bit_encoder)
    {
    }

    /** Encode both the header and the data. */
    template<std::input_iterator InIt, typename... InItEnd>
        requires (sizeof...(InItEnd) <= 1)
    Stat encode(const DeflateParams& params, InIt in_it, InItEnd... in_it_end)
    {
        Stat s = success;
        (s = encode_header_bits(params.header_bits)) &&
        (s = encode_data(params, in_it, in_it_end...));
        return s;
    }

    /** Encode the header bits. */
    inline Stat encode_header_bits(DeflateHeaderBits header_bits)
    {
        // non-compressed blocks are not supported (BTYPE=00)
        // compression with static Huffman codes are not supported (BTYPE=01)
        // supporting only a compression with dynamic Huffman codes (BTYPE=10)
        if (bit_encoder.template encode_bits<3>(static_cast<uint8_t>(header_bits))) [[likely]]
            return success;
        else
            return "failed encoding header bits";
    }

    /** Encode data only. */
    template<std::input_iterator InIt, typename... InItEnd>
        requires (sizeof...(InItEnd) <= 1)
    inline Stat encode_data(const DeflateParams& params, InIt in_it, InItEnd... in_it_end)
    {
        return huffman_encode(lz77_encode(params.lz77_encoder_params, in_it, in_it_end...));
    }

private:
    /** Apply LZ77 encoding on the data and convert it into an intermediate representation. */
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

    /** Apply Huffman encoding on the intermediate data. */
    Stat huffman_encode(const IntermediateData& data)
    {
        std::array<CodeLen, litlen_alphabet_size + dist_alphabet_size> code_lens_storage {};
        auto [litlen_code_lens, dist_code_lens] = huffman_find_code_lens(data, code_lens_storage);

        std::span both_code_lens {code_lens_storage.data(), litlen_code_lens.size() + dist_code_lens.size()};

        const std::size_t nr_litlen_codes = litlen_code_lens.size();
        const std::size_t nr_dist_codes = dist_code_lens.size();

        Stat s = huffman_encode_code_lens(nr_litlen_codes, nr_dist_codes, both_code_lens);
        if (s.failed()) [[unlikely]]
            return {"failed encoding code lengths", s};
        if (huffman_encode_syms(data, litlen_code_lens, dist_code_lens)) [[likely]]
            return success;
        else
            return "failed encoding literal/length and distance symbols and extra bits";
    }

    /** Find lengths of the literal/length and distance codes suitable for the intermediate data. */
    std::tuple<std::span<CodeLen>, std::span<CodeLen>>
        huffman_find_code_lens(const IntermediateData& data, std::span<CodeLen> code_lens)
    {
        std::array<Freq, litlen_alphabet_size> litlen_freq {};
        std::array<Freq, dist_alphabet_size> dist_freq {};
        count_freqs(data, litlen_freq, dist_freq);

        std::span<CodeLen> litlen_code_lens (code_lens.data(), litlen_alphabet_size);
        Huffman_::template
            find_code_lengths<litlen_alphabet_size>(litlen_freq.begin(), litlen_code_lens.data());
        assert(Huffman_::validate_code_lens(litlen_code_lens.begin(), litlen_code_lens.end()));
        litlen_code_lens = strip_trailing_zeros(litlen_code_lens, literal_term_alphabet_size);

        std::span<CodeLen> dist_code_lens (code_lens.data() + litlen_code_lens.size(), dist_alphabet_size);
        Huffman_::template
            find_code_lengths<dist_alphabet_size>(dist_freq.begin(), dist_code_lens.data());
        assert(Huffman_::validate_code_lens(dist_code_lens.begin(), dist_code_lens.end()));
        dist_code_lens = strip_trailing_zeros(dist_code_lens, 1);

        return std::make_tuple(litlen_code_lens, dist_code_lens);
    }

    /** Encode the code lengths using code length Huffman coding. */
    Stat huffman_encode_code_lens(std::size_t nr_litlen_codes, std::size_t nr_dist_codes,
            std::span<const CodeLen> both_code_lens)
    {
        auto dh_encoder = DHuffman::make_encoder(bit_encoder);
        Stat s = success;
        (s = encode_nr_codes(nr_litlen_codes, nr_dist_codes)) &&
        (s = dh_encoder.encode_code_lens(both_code_lens.begin(), both_code_lens.end()));
        return s;
    }

    /** Encode the number of literal/length and distance codes. */
    Stat encode_nr_codes(std::size_t nr_litlen_codes, std::size_t nr_dist_codes)
    {
        if (encode_nr_litlen_codes(nr_litlen_codes) && encode_nr_dist_codes(nr_dist_codes))
            return success;
        else [[unlikely]]
            return "failed encoding the numbers of literal/length and distance code lengths";
    }

    /** Encode the number of literal/length codes. */
    bool encode_nr_litlen_codes(std::size_t nr_litlen_codes)
    {
        assert(nr_litlen_codes - literal_term_alphabet_size <= 31);
        return bit_encoder.template encode_bits<5>(nr_litlen_codes - literal_term_alphabet_size);
    }

    /** Encode the number of distance codes. */
    bool encode_nr_dist_codes(std::size_t nr_dist_codes)
    {
        assert(nr_dist_codes - 1 <= 31);
        return bit_encoder.template encode_bits<5>(nr_dist_codes - 1);
    }

    /** Huffman-encode symbols in the intermediate data, given the code lengths. */
    bool huffman_encode_syms(const IntermediateData& data,
            std::span<const CodeLen> litlen_code_lens, std::span<const CodeLen> dist_code_lens)
    {
        std::array<Code, litlen_alphabet_size> litlen_codes_storage;
        std::array<Code, dist_alphabet_size> dist_codes_storage;
        auto [litlen_codes, dist_codes] = gen_codes(litlen_code_lens, dist_code_lens,
                litlen_codes_storage, dist_codes_storage);

        bool successful_so_far = true;

        auto litlen_encoder = Huffman_::make_encoder(
                litlen_codes.data(), litlen_code_lens.data(), bit_encoder);
        auto dist_encoder = Huffman_::make_encoder(
                dist_codes.data(), dist_code_lens.data(), bit_encoder);

        for (auto it = data.begin(); it != data.end() && successful_so_far; ++it) {
            PackedToken token = *it;
            if (token.is_literal()) {
                const LitLenSym sym = to_litlen(token.get_literal());
                successful_so_far = litlen_encoder.encode_sym(sym);
            } else if (token.is_len_dist()) {
                ++it;
                if (it == data.end())
                    throw Exception<Encoder>("expected a distance extra bits token");

                PackedToken extra = *it;

                const LenSym len_sym = token.get_len_sym();
                const LitLenSym litlen_sym = to_litlen(len_sym);
                const LenExtra len_extra = token.get_len_extra();
                const DistSym dist_sym = token.get_dist_sym();
                const DistExtra dist_extra = extra.get_dist_extra();

                const std::size_t len_extra_bits = DeflateLZ77::get_nr_len_extra_bits(len_sym);
                const std::size_t dist_extra_bits = DeflateLZ77::get_nr_dist_extra_bits(dist_sym);

                successful_so_far =
                    litlen_encoder.encode_sym(litlen_sym) &&
                    bit_encoder.encode_bits(len_extra, len_extra_bits) &&
                    dist_encoder.encode_sym(dist_sym) &&
                    bit_encoder.encode_bits(dist_extra, dist_extra_bits);
            } else [[unlikely]] {
                throw Exception<Encoder>("invalid token");
            }
        }
        successful_so_far = successful_so_far &&
            bit_encoder.encode_bits(litlen_codes[term_sym], litlen_code_lens[term_sym]);
        return successful_so_far;
    }

    template<std::integral T>
    static std::span<T> strip_trailing_zeros(std::span<T> code_lens, std::size_t min_size)
    {
        std::size_t size = code_lens.size();
        for (; size > min_size && code_lens[size - 1] == 0; --size);
        return code_lens.first(size);
    }

    /** Count frequencies for each literal/length and distance symbols in the intermediate data. */
    static void count_freqs(const IntermediateData& data,
            std::array<Freq, litlen_alphabet_size>& litlen_freq,
            std::array<Freq, dist_alphabet_size>& dist_freq)
    {
        for (std::size_t i = 0; i < data.size(); ++i) {
            DeflateLZ77::PackedToken token = data[i];
            if (token.is_literal()) {
                ++litlen_freq[to_litlen(token.get_literal())];
            } else if (token.is_len_dist()) {
                const LitLenSym litlen_sym = to_litlen(token.get_len_sym());
                const DistSym dist_sym = token.get_dist_sym();

                assert(litlen_sym < litlen_alphabet_size);
                assert(dist_sym < dist_alphabet_size);

                ++litlen_freq[litlen_sym];
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

/** The Deflate::Decoder class. Supports decoding data, header_bits, and both at once. */
template<DeflatePolicy Policy>
template<typename Char, std::size_t char_size, std::input_iterator InIt, typename... InItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(InItEnd) <= 1)
class Deflate<Policy>::Decoder {
public:
    using Stat = StatStr;
    using BitDecoder = misc::BitDecoder<Char, char_size, InIt, InItEnd...>;

    explicit Decoder(BitDecoder& bit_decoder) : bit_decoder(bit_decoder)
    {
    }

    /** Decode both the header and the data. */
    template<std::output_iterator<char> OutIt, typename... OutItEnd>
    std::tuple<OutIt, DeflateHeaderBits, Stat> decode(OutIt out_it, OutItEnd... out_it_end)
    {
        auto [header_bits, s] = decode_header_bits();
        if (s.failed()) [[unlikely]]
            return std::make_tuple(out_it, header_bits, std::move(s));

        const DeflateHeaderBits block_type = header_bits & DeflateHeaderBits::TypeMask;
        if (block_type != DeflateHeaderBits::DynamicHuffman)
            return std::make_tuple(out_it, header_bits, "unsupported block type");

        std::tie(out_it, s) = decode_data(out_it, out_it_end...);
        return std::make_tuple(out_it, header_bits, std::move(s));
    }

    /** Decode the header bits. */
    std::tuple<DeflateHeaderBits, Stat> decode_header_bits()
    {
        std::bitset<3> header_bits {};
        const bool s = bit_decoder.decode_bits(header_bits);
        Stat ss = s ? success : StatStr("failed decoding header bits");
        return std::make_tuple(static_cast<DeflateHeaderBits>(header_bits.to_ullong()), std::move(ss));
    }

    /** Decode data only. */
    template<std::output_iterator<char> OutIt, typename... OutItEnd>
    std::tuple<OutIt, Stat> decode_data(OutIt out_it, OutItEnd... out_it_end)
    {
        return lz77_huffman_decode(out_it, out_it_end...);
    }

private:
    /** Decode the data by Huffman and LZ77. Unlike encoding, decoding is not split
     * into separate steps for LZ77 and Huffman coding, but rather each LZ77 token
     * that is decoded by Huffman, is immediately decoded by LZ77. */
    template<std::output_iterator<char> OutIt, typename ...OutItEnd>
    std::tuple<OutIt, Stat> lz77_huffman_decode(OutIt out_it, OutItEnd... out_it_end)
    {
        std::array<CodeLen, litlen_alphabet_size + dist_alphabet_size> code_lens {};
        auto [litlen_code_lens, dist_code_lens, s] = huffman_decode_code_lens(code_lens);
        if (s.failed()) [[unlikely]]
            return std::make_tuple(out_it, std::exchange(s, success));
        else
            return lz77_huffman_decode_syms(litlen_code_lens, dist_code_lens, out_it, out_it_end...);
    }

    /** Decode Huffman code length codes. */
    std::tuple<std::span<CodeLen>, std::span<CodeLen>, Stat>
        huffman_decode_code_lens(std::span<CodeLen> code_lens)
    {
        std::tuple<std::span<CodeLen>, std::span<CodeLen>, Stat> result;
        auto& [litlen_code_lens, dist_code_lens, s] = result;

        s = success;

        std::size_t nr_litlen_codes = 0, nr_dist_codes = 0;
        s = decode_nr_codes(nr_litlen_codes, nr_dist_codes);
        if (s.failed()) [[unlikely]]
            return result;

        auto dh_decoder = DHuffman::make_decoder(bit_decoder);
        const std::size_t nr_codes = (nr_litlen_codes + nr_dist_codes);
        s = dh_decoder.decode_code_lens(code_lens.begin(), code_lens.begin() + nr_codes);

        litlen_code_lens = code_lens.first(nr_litlen_codes);
        dist_code_lens = code_lens.subspan(nr_litlen_codes, nr_dist_codes);

        return result;
    }

    /** Decode the number of literal/length and distance codes. */
    Stat decode_nr_codes(std::size_t& nr_litlen_codes, std::size_t& nr_dist_codes)
    {
        if (decode_nr_litlen_codes(nr_litlen_codes) && decode_nr_dist_codes(nr_dist_codes)) [[likely]]
            return success;
        else
            return "failed decoding the numbers of literal/length and distance code lengths";
    }

    /** Decode the number of literal/length codes. */
    inline bool decode_nr_litlen_codes(std::size_t& nr_litlen_codes)
    {
        bool s = bit_decoder.template decode_bits<5>(nr_litlen_codes);
        nr_litlen_codes += literal_term_alphabet_size;
        return s;
    }

    /** Decode the number of distance codes. */
    inline bool decode_nr_dist_codes(std::size_t& nr_dist_codes)
    {
        bool s = bit_decoder.template decode_bits<5>(nr_dist_codes);
        nr_dist_codes += 1;
        return s;
    }

    /** Decode the symbols in the input data by Huffman and LZ77,
     * based on the literal/length and distance code lengths. */
    template<std::output_iterator<char> OutIt, typename ...OutItEnd>
    std::tuple<OutIt, Stat> lz77_huffman_decode_syms(
            std::span<const CodeLen> litlen_code_lens,
            std::span<const CodeLen> dist_code_lens,
            OutIt out_it, OutItEnd... out_it_end)
    {
        Stat s = success;

        if (not Huffman_::validate_code_lens(litlen_code_lens.begin(), litlen_code_lens.end())) [[unlikely]] {
            s = "invalid literal/length code lengths decoded";
            return std::make_tuple(out_it, std::move(s));
        }
        if (not Huffman_::validate_code_lens(dist_code_lens.begin(), dist_code_lens.end())) [[unlikely]] {
            s = "invalid distance code lengths decoded";
            return std::make_tuple(out_it, std::move(s));
        }

        HuffmanTree litlen_tree, dist_tree;
        std::tie(litlen_tree, dist_tree, s) = huffman_build_trees(litlen_code_lens, dist_code_lens);
        if (s.failed()) [[unlikely]]
            return std::make_tuple(out_it, std::move(s));

        return lz77_huffman_decode_syms(litlen_tree.get_root(), dist_tree.get_root(), out_it, out_it_end...);
    }

    /** Decode the symbols in the input data by Huffman and LZ77,
     * based on the literal/length and distance Huffman trees. */
    template<std::output_iterator<char> OutIt, typename ...OutItEnd>
    std::tuple<OutIt, Stat> lz77_huffman_decode_syms(
            const HuffmanTreeNode *litlen_tree_node, const HuffmanTreeNode *dist_tree_node,
            OutIt out_it, OutItEnd... out_it_end)
    {
        auto lz77_decoder = DeflateLZ77::make_decoder(out_it, out_it_end...);
        Stat s = success;

        auto litlen_decoder = Huffman_::make_decoder(litlen_tree_node, bit_decoder);
        auto dist_decoder = Huffman_::make_decoder(dist_tree_node, bit_decoder);

        while (not lz77_decoder.is_finished() && bit_decoder.is_valid()) {
            std::optional<LitLenSym> opt_litlen_sym = litlen_decoder.decode_sym();
            if (!opt_litlen_sym) {
                s = {"failed decoding literal/length symbol"};
                break;
            }

            const LitLenSym litlen_sym = *opt_litlen_sym;

            if (is_literal(litlen_sym)) {
                lz77_decoder.decode_once(get_literal(litlen_sym));
            } else if (is_term(litlen_sym)) {
                break;
            } else if (is_len_sym(litlen_sym)) {
                const LenSym len_sym = get_len_sym(litlen_sym);
                LenExtra len_extra = 0;
                DistSym dist_sym = 0;
                DistExtra dist_extra = 0;

                std::tie(len_extra, dist_sym, dist_extra, s) =
                    huffman_decode_len_extra_and_dist(len_sym, dist_decoder);
                if (s.failed()) [[unlikely]]
                    break;

                s = lz77_decoder.decode_once(len_sym, len_extra, dist_sym, dist_extra);
                if (s.failed()) [[unlikely]] {
                    s = {"failed decoding LZ77 length/distance pair", s};
                    break;
                }
            } else [[unlikely]] {
                s = "invalid literal/length symbol decoded";
                break;
            }
        }

        return std::make_tuple(lz77_decoder.get_it(), std::exchange(s, success));
    }

    /** Huffman-decode the length extra bits and the distance symbol and extra bits.
     * This is needed when the first decoded literal/length symbol is a length symbol. */
    std::tuple<LenExtra, DistSym, DistExtra, Stat> huffman_decode_len_extra_and_dist(
            LenSym len_sym, auto& decoder)
    {
        std::tuple<LenExtra, DistSym, DistExtra, Stat> result = std::make_tuple(0, 0, 0, success);
        auto& [len_extra, dist_sym, dist_extra, s] = result;

        const std::size_t len_extra_bits = DeflateLZ77::get_nr_len_extra_bits(len_sym);
        if (not bit_decoder.decode_bits(len_extra, len_extra_bits)) [[unlikely]] {
            s = "failed decoding length extra bits";
            return result;
        }

        std::optional<DistSym> opt_dist_sym = decoder.decode_sym();
        if (!opt_dist_sym) [[unlikely]] {
            s = "failed decoding distance symbol";
            return result;
        }

        dist_sym = *opt_dist_sym;
        if (not is_valid_dist_sym(dist_sym)) [[unlikely]] {
            s = "invalid distance symbol decoded";
            return result;
        }

        const std::size_t dist_extra_bits = DeflateLZ77::get_nr_dist_extra_bits(dist_sym);
        if (not bit_decoder.decode_bits(dist_extra, dist_extra_bits)) {
            s = "failed decoding distance extra bits";
            return result;
        }
        assert(dist_extra < (DistExtra(1) << DeflateLZ77::get_nr_dist_extra_bits(dist_sym)));

        s = success;
        return result;
    }

    BitDecoder& bit_decoder;
};

/** Deflate data and write it to the bit encoder.
 * Return the input iterator and status at the point when encoding stopped. */
template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
         DeflatePolicy Policy = BasicDeflatePolicy,
         std::input_iterator InIt = typename std::vector<char>::iterator,
         std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator, typename ...OutItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(OutItEnd) <= 1)
inline std::tuple<InIt, StatStr> deflate(const DeflateParams& params,
        misc::BitEncoder<char, CHAR_BIT, OutIt, OutItEnd...>& bit_encoder,
        InIt in_it, InIt in_it_end)
{
    auto encoder = Deflate<Policy>::make_encoder(bit_encoder);
    return std::make_tuple(in_it_end, encoder.encode(params, in_it, in_it_end));
}

/** Inflate data by reading it form the bit decoder.
 * Return the output iterator, header bits and status at the point when decoding stopped. */
template<typename Char = char, std::size_t char_size = sizeof(Char) *  CHAR_BIT,
         DeflatePolicy Policy = BasicDeflatePolicy,
         std::output_iterator<char> OutIt = typename std::vector<char>::iterator,
         std::input_iterator InIt = typename std::vector<Char>::iterator, typename ...InItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(InItEnd) <= 1)
std::tuple<OutIt, DeflateHeaderBits, StatStr> inflate(OutIt out_it, OutIt out_it_end,
        misc::BitDecoder<Char, char_size, InIt, InItEnd...>& bit_decoder)
{
    return Deflate<Policy>::make_decoder(bit_decoder).decode(out_it, out_it_end);
}

/** Deflate data. Return (InIt, OutIt, Stat) triple at the point when encoding stopped. */
template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
         DeflatePolicy Policy = BasicDeflatePolicy,
         std::input_iterator InIt = typename std::vector<Char>::iterator,
         std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator, typename ...OutItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(OutItEnd) <= 1)
std::tuple<InIt, OutIt, StatStr>
    deflate(const DeflateParams& params, InIt in_it, InIt in_it_end, OutIt out_it, OutItEnd... out_it_end)
{
    auto bit_encoder = misc::make_bit_encoder<Char, char_size>(out_it, out_it_end...);

    StatStr s;
    std::tie(in_it, s) = deflate(params, bit_encoder, in_it, in_it_end);
    out_it = bit_encoder.get_it();
    if (s.failed())
        return std::make_tuple(in_it, out_it, std::move(s));

    bit_encoder.finalize();
    out_it = bit_encoder.get_it();
    return std::make_tuple(in_it, out_it, success);
}

/** Decode data. Return (OutIt, InIt, DeflateHeaderBits, Stat) tuple at the point when decoding stopped. */
template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
         DeflatePolicy Policy = BasicDeflatePolicy,
         std::output_iterator<char> OutIt = typename std::vector<char>::iterator,
         std::input_iterator InIt = typename std::vector<Char>::iterator, typename ...InItEnd>
    requires ((sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT) && sizeof...(InItEnd) <= 1)
std::tuple<OutIt, InIt, DeflateHeaderBits, StatStr>
    inflate(OutIt out_it, OutIt out_it_end, InIt in_it, InItEnd... in_it_end)
{
    auto bit_decoder = misc::make_bit_decoder<Char, char_size>(in_it, in_it_end...);
    StatStr s;
    DeflateHeaderBits header_bits;
    std::tie(out_it, header_bits, s) = inflate(out_it, out_it_end, bit_decoder);
    in_it = bit_decoder.get_it();
    return std::make_tuple(out_it, in_it, header_bits, std::move(s));
}

}
