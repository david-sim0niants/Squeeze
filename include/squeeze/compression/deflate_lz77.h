#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

#include "lz77.h"

namespace squeeze::compression {

/* DeflateLZ77 interface defines additional set of LZ77 coding functionalities
 * as specified by the DEFLATE specification (RFC1951).
 * Provides methods for encoding data in a DEFLATE-specified Literal/Length alphabet and Distance alphabet
 * symbols and extra bits. */
class DeflateLZ77 {
public:
    struct Policy {
        using Sym = char;
        static constexpr std::size_t min_len = 3;
    };

    using LZ77_ = LZ77<Policy>;
    using Literal = LZ77_::Sym;
    static constexpr std::size_t min_len = LZ77_::min_len;
    using Token = LZ77_::Token;

    /* 5 bit length symbol that maps from [0-28] to [257-285] length symbols of the
     * Literal/Length alphabet per the DEFLATE spec. In short: 'LenSym = LiteralLengthSym - 257' */
    using LenSym = uint8_t;
    /* Up to 5 bits for the length extra bits */
    using LenExtra = uint8_t;
    /* 5 bit distance symbol. */
    using DistSym = uint8_t;
    /* Up to 13 bits of distance extra bits */
    using DistExtra = uint16_t;

    /* Represents a range of lengths [3-258] */
    using PackedLen = uint8_t;
    /* Represents a range of distances [1-32768] */
    using PackedDist = uint16_t;

    class PackedToken;

    template<std::input_iterator InIt, typename... InItEnd>
    class Encoder;
    template<std::output_iterator<Literal> OutIt, typename... OutItEnd>
    class Decoder;

    inline static PackedLen pack_len(std::size_t len)
    {
        assert(len <= lookahead_size && "length too large");
        assert(len >= min_len && "length too small");
        return static_cast<PackedLen>(len - min_len);
    }

    inline static std::size_t unpack_len(PackedLen p_len)
    {
        return static_cast<std::size_t>(p_len) + min_len;
    }

    inline static PackedDist pack_dist(std::size_t dist)
    {
        assert(dist <= search_size && "too large distance");
        assert(dist > 0 && "zero-distance");
        return static_cast<PackedDist>(dist - 1);
    }

    inline static std::size_t unpack_dist(PackedDist dist)
    {
        return static_cast<std::size_t>(dist) + 1;
    }

    /* Does some convoluted math to encode a normal length into a length symbol and extra bits pair. */
    static std::pair<LenSym, LenExtra> encode_len(std::size_t len)
    {
        const PackedLen p_len = pack_len(len);
        if (p_len <= 7)
            return std::make_pair(p_len, 0);
        else if (p_len == 255)
            return std::make_pair(max_len_sym, 0);
        const int p = (std::bit_width(p_len) - 1) - 2;
        return std::make_pair(4 * p + (p_len >> p), p_len & PackedLen((1 << p) - 1));
    }

    /* Does some convoluted math to decode a length symbol and extra bits pair into a normal length. */
    static Error<DeflateLZ77> decode_len(LenSym len_sym, LenExtra len_extra, std::size_t& len)
    {
        if (len_sym > max_len_sym) [[unlikely]]
            return "invalid length symbol";

        if (len_sym <= 7) {
            len = unpack_len(len_sym);
            return success;
        } else if (len_sym == max_len_sym) {
            len = unpack_len(255);
            return success;
        }

        const auto p = len_sym / 4 - 1;
        if (len_extra >= (1 << p)) [[unlikely]]
            return "invalid length extra bits";

        len = unpack_len((static_cast<PackedLen>(len_sym % 4 + 4) << p) + len_extra);
        return success;
    }

    /* Does some convoluted math to encode a normal distance into a distance symbol and extra bits pair. */
    static std::pair<DistSym, DistExtra> encode_dist(std::size_t dist)
    {
        const PackedDist p_dist = pack_dist(dist);
        if (p_dist <= 3)
            return std::make_pair(p_dist, 0);
        const int p = (std::bit_width(p_dist) - 1) - 1;
        return std::make_pair(2 * p + (p_dist >> p), (p_dist & ((1 << p) - 1)));
    }

    /* Does some convoluted math to decode a distance symbol and extra bits pair into a normal distance. */
    static Error<DeflateLZ77> decode_dist(DistSym dist_sym, DistExtra dist_extra, std::size_t& dist)
    {
        if (dist_sym > max_dist_sym) [[unlikely]]
            return "invalid distance symbol";

        if (dist_sym <= 3) {
            dist = unpack_dist(dist_sym);
            return success;
        }

        const auto p = dist_sym / 2 - 1;
        if (dist_extra >= (1 << p)) [[unlikely]]
            return "invalid distance extra bits";

        dist = unpack_dist((static_cast<PackedDist>(dist_sym % 2 + 2) << p) + dist_extra);
        return success;
    }

    inline static constexpr std::size_t get_sym_extra_bits_len(LenSym len_sym)
    {
        if (len_sym > max_len_sym) [[unlikely]]
            throw Exception<DeflateLZ77>("invalid length symbol");
        else if (len_sym <= 7 || len_sym == max_len_sym)
            return 0;
        else
            return len_sym / 4 - 1;
    }

    inline static constexpr std::size_t get_dist_extra_bits_len(DistSym dist_sym)
    {
        if (dist_sym > max_dist_sym) [[unlikely]]
            throw Exception<DeflateLZ77>("invalid distance symbol");
        else if (dist_sym <= 1)
            return 0;
        else
            return dist_sym / 2 - 1;
    }

    /* Make an encoder from the provided input iterator(s). */
    template<std::input_iterator InIt, typename... InItEnd>
    inline static auto make_encoder(InIt in_it, InItEnd... in_it_end)
    {
        return Encoder<InIt, InItEnd...>(in_it, in_it_end...);
    }

    /* Make an encoder from the provided params and input iterator(s). */
    template<std::input_iterator InIt, typename... InItEnd>
    inline static auto make_encoder(const LZ77EncoderParams& params, InIt in_it, InItEnd... in_it_end)
    {
        return Encoder<InIt, InItEnd...>(in_it, in_it_end...);
    }

    /* Make a decoder from the provided output iterator(s). */
    template<std::output_iterator<Literal> OutIt, typename... OutItEnd>
    inline static auto make_decoder(OutIt out_it, OutItEnd... out_it_end)
    {
        return Decoder<OutIt, OutItEnd...>(out_it, out_it_end...);
    }

    /* Max length symbol */
    static constexpr LenSym max_len_sym = 28;
    /* Max distance symbol */
    static constexpr DistSym max_dist_sym = 29;

    /* Search window size. */
    static constexpr std::size_t search_size = 1 << 15;
    /* Lookahead window size. */
    static constexpr std::size_t lookahead_size = 258;
};

/* Representation of an LZ77 token in a packed 16-bit that is more memory-efficient and
 * can be used as an intermediate LZ77 code for further encoding.
 * Supports four representations: None, [LenExtra/LenSym/DistSym], DistExtra, and Literal.
 * A None value is represented with the 13th and 14th bits on (0x6000).
 * A Len/Dist pair is split into two packed tokens. The first holds 5 bits for the
 * length symbol, 5 bits for the distance symbol, 5 bits of length extra bits, and a bit
 * indicating it's the first token of a Len/Dist pair. The second packed token stores
 * up to 13 bits of distance extra bits. A literal is represented as is, with the upper
 * 8 bits set to zero and the lower 8 bits storing the literal value.
 * +-----------+------------------------------------------------------------+
 * | Type      |                      Bit arrangement                       |
 * +-----------+------------------------------------------------------------+
 * | None      |                           0x6000                           |
 * +-----------+------------------------------------------------------------+
 * | Len/Dist  |  0x8000 | (len_extra << 10) | (len_sym << 5) | (dist_sym)  |
 * +-----------+------------------------------------------------------------+
 * | DistExtra |                 dist_extra (lower 13 bits)                 |
 * +-----------+------------------------------------------------------------+
 * | Literal   |                  literal (lower 8 bits)                    |
 * +-----------+------------------------------------------------------------+
 */
class DeflateLZ77::PackedToken {
public:
    /* Construct a none packed token. */
    constexpr PackedToken() = default;

    /* Construct a literal packed token. */
    constexpr PackedToken(Literal literal) : data(std::make_unsigned_t<Literal>(literal))
    {
    }

    /* Construct a 'Len/Dist pair without distance extra bits' packed token. */
    constexpr PackedToken(LenExtra len_extra, LenSym len_sym, DistSym dist_sym)
        : data(len_dist_mark | (len_extra << 10) | (len_sym << 5) | dist_sym)
    {
        assert(len_extra < 32);
        assert(len_sym < 32);
        assert(dist_sym < 32);
    }

    /* Construct a 'distance extra bits' packed token. */
    constexpr PackedToken(DistExtra dist_extra) : data(dist_extra & dist_extra_mask)
    {
    }

    /* Check if none. */
    constexpr inline bool is_none() const noexcept
    {
        return data == none_mark;
    }

    /* Check if a symbol token. */
    constexpr inline bool is_literal() const noexcept
    {
        return !(data & len_dist_mark);
    }

    /* Check if first token of a len/dist pair. */
    constexpr inline bool is_len_dist() const noexcept
    {
        return !!(data & len_dist_mark);
    }

    /* Get length symbol. */
    constexpr inline LenSym get_len_sym() const noexcept
    {
        return static_cast<LenSym>((data >> 5) & sym_mask);
    }

    /* Get length extra bits. */
    constexpr inline LenExtra get_len_extra() const noexcept
    {
        return static_cast<LenExtra>((data >> 10) & sym_mask);
    }

    /* Get distance symbol. */
    constexpr inline DistSym get_dist_sym() const noexcept
    {
        return static_cast<DistSym>(data & sym_mask);
    }

    /* Get distance extra bits. */
    constexpr inline DistExtra get_dist_extra() const noexcept
    {
        return static_cast<DistExtra>(data & dist_extra_mask);
    }

    /* Get literal. */
    constexpr inline Literal get_literal() const noexcept
    {
        return static_cast<Literal>(data & literal_mask);
    }

    /* Get the underlying raw data. */
    constexpr inline uint16_t get_raw() const noexcept
    {
        return data;
    }

private:
    static constexpr uint16_t none_mark = 0x6000;
    static constexpr uint16_t len_dist_mark = 0x8000;
    static constexpr uint16_t sym_mask = 0x1F;
    static constexpr uint16_t len_extra_mask = 0x1F;
    static constexpr uint16_t dist_extra_mask = 0x1FFF;
    static constexpr uint16_t literal_mask = 0xFF;

    uint16_t data = none_mark;
};

/* The DeflateLZ77::Encoder class. Internally uses the LZ77 Encoder and converts LZ77 tokens
 * into packed tokens. */
template<std::input_iterator InIt, typename... InItEnd>
class DeflateLZ77::Encoder {
public:
    /* Construct from the provided input iterator(s). */
    explicit Encoder(InIt in_it, InItEnd... in_it_end)
        : internal(search_size, lookahead_size, in_it, in_it_end...)
    {
    }

    /* Construct from the provided params and input iterator(s). */
    explicit Encoder(const LZ77EncoderParams& params, InIt in_it, InItEnd... in_it_end)
        : internal(params, search_size, lookahead_size, in_it, in_it_end...)
    {
    }

    /* Construct as a continuation of another encoder with new iterator(s). */
    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    Encoder(const Encoder<PrevInIt, PrevInItEnd...>& prev, InIt in_it, InItEnd... in_it_end)
        :   internal(prev.internal, in_it, in_it_end...)
    {
    }

    /* Construct as a continuation of another encoder with new iterator(s). */
    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    Encoder(Encoder<PrevInIt, PrevInItEnd...>&& prev, InIt in_it, InItEnd... in_it_end)
        :   internal(std::move(prev.internal), in_it, in_it_end...)
    {
    }

    /* Encodes current data by encoding internally encoded tokens into packed tokens.
     * For len/dist pair, two packed tokens are created. */
    std::pair<PackedToken, PackedToken> encode_once()
    {
        const Token token = internal.encode_once();
        switch (token.get_type()) {
        case Token::None:
            return std::make_pair(PackedToken(), PackedToken());
        case Token::Symbol:
            return std::make_pair(PackedToken(token.get_sym()), PackedToken());
        case Token::LenDist:
        {
            const auto [len_sym, len_extra] = encode_len(token.get_len());
            const auto [dist_sym, dist_extra] = encode_dist(token.get_dist());
            return std::make_pair(PackedToken(len_extra, len_sym, dist_sym), dist_extra);
        }
        default:
            throw Exception<Encoder>("invalid token type");
        }
    }

    /* Encode a sequence of packed tokens from internally encoded tokens.
     * If an extra token failed to be written (when the output iterator reached the end),
     * it's returned from the function, otherwise a none token is returned, so error check
     * can be done by checking if the returned token is a none token. */
    template<std::output_iterator<PackedToken> OutIt, typename... OutItEnd>
    PackedToken encode(OutIt out_it, OutItEnd... out_it_end)
    {
        for (misc::Sequence seq(out_it, out_it_end...); seq.is_valid(); ++seq.it) {
            auto [main, extra] = encode_once();
            if (main.is_none())
                break;
            *seq.it = main;
            if (extra.is_none())
                continue;
            ++seq.it;
            if (seq.is_valid()) [[likely]]
                *seq.it = extra;
            else
                return extra;
        }
        return {};
    }

    /* Check if the encoder has finished encoding with the current sequence. */
    inline bool is_finished() const noexcept
    {
        return internal.is_finished();
    }

    /* Get current iterator. */
    inline InIt& get_it() noexcept
    {
        return internal.get_it();
    }

    /* Get current iterator. */
    inline const InIt& get_it() const noexcept
    {
        return internal.get_it();
    }

    /* Continue by using different iterator(s). */
    template<std::input_iterator NextInIt, typename... NextInItEnd>
    inline auto continue_by(NextInIt in_it, NextInItEnd... in_it_end) const &
    {
        return Encoder<NextInIt, NextInItEnd...>(*this, in_it, in_it_end...);
    }

    /* Continue by using different iterator(s). */
    template<std::input_iterator NextInIt, typename... NextInItEnd>
    inline auto continue_by(NextInIt in_it, NextInItEnd... in_it_end) &&
    {
        return Encoder<NextInIt, NextInItEnd...>(std::move(*this), in_it, in_it_end...);
    }

private:
    /* Internal LZ77 encoder. */
    LZ77_::Encoder<InIt, InItEnd...> internal;
};

/* The DeflateLZ77::Decoder class. Converts Literal/Length/Distance symbols and Length/Distance
 * extra bits back to normal values and internally uses the LZ77::Decoder to decode them. */
template<std::output_iterator<DeflateLZ77::Literal> OutIt, typename... OutItEnd>
class DeflateLZ77::Decoder {
public:
    /* Construct from the provided output iterator(s). */
    explicit Decoder(OutIt out_it, OutItEnd... out_it_end)
        : internal(search_size, lookahead_size, out_it, out_it_end...)
    {
    }

    /* Construct as a continuation of another decoder with new iterator(s). */
    template<std::output_iterator<Literal> PrevOutIt, typename... PrevOutItEnd>
    Decoder(const Decoder<PrevOutIt, PrevOutItEnd...>& prev, OutIt out_it, OutItEnd... out_it_end)
        : internal(prev.internal)
    {
    }

    /* Construct as a continuation of another decoder with new iterator(s). */
    template<std::output_iterator<Literal> PrevOutIt, typename... PrevOutItEnd>
    Decoder(Decoder<PrevOutIt, PrevOutItEnd...>&& prev, OutIt out_it, OutItEnd... out_it_end)
        : internal(std::move(prev.internal))
    {
    }

    /* Decode a literal. */
    inline void decode_once(Literal literal)
    {
        internal.decode_once(literal);
    }

    /* Decode a len/dist pair from a length symbol and extra bits, distance symbol and its extra bits. */
    inline Error<Decoder>
        decode_once(LenSym len_sym, LenExtra len_extra, DistSym dist_sym, DistExtra dist_extra)
    {
        std::size_t len, dist;
        Error<DeflateLZ77> e;
        if ((e = decode_len(len_sym, len_extra, len)).failed() or
            (e = decode_dist(dist_sym, dist_extra, dist)).failed())
            return e;
        else
            return internal.decode_once(len, dist);
    }

    /* Check if the decoder has finished processing tokens. */
    inline bool is_finished() const noexcept
    {
        return internal.is_finished();
    }

    /* Get current iterator. */
    inline OutIt& get_it() noexcept
    {
        return internal.get_it();
    }

    /* Get current iterator. */
    inline const OutIt& get_it() const noexcept
    {
        return internal.get_it();
    }

    /* Continue by using different iterator(s). */
    template<std::output_iterator<Literal> NextOutIt, typename... NextOutItEnd>
    inline auto continue_by(NextOutIt in_it, NextOutItEnd... in_it_end) const &
    {
        return Decoder<NextOutIt, NextOutItEnd...>(*this, in_it, in_it_end...);
    }

    /* Continue by using different iterator(s). */
    template<std::output_iterator<Literal> NextOutIt, typename... NextOutItEnd>
    inline auto continue_by(NextOutIt in_it, NextOutItEnd... in_it_end) &&
    {
        return Decoder<NextOutIt, NextOutItEnd...>(std::move(*this), in_it, in_it_end...);
    }

private:
    /* Internal LZ77 decoder. */
    LZ77_::Decoder<OutIt, OutItEnd...> internal;
};

}
