#pragma once

#include <cassert>
#include <cstdint>
#include <climits>
#include <iterator>
#include <bitset>
#include <utility>
#include <vector>

#include "squeeze/utils/iterator.h"
#include "sequence.h"

namespace squeeze::misc {

/* This is a class used for encoding bits. Internally handles all the necessary bit operations
 * to provide a bit-writing medium interface. Writes bits on an output sequence defined by the
 * provided output iterator(s) */
template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
    std::output_iterator<Char> OutIt = typename std::vector<Char>::iterator, typename OutItEnd = void>
    requires (sizeof(Char) <= sizeof(unsigned long long) * CHAR_BIT and
              (std::same_as<OutItEnd, void> or std::same_as<OutItEnd, OutIt>))
class BitEncoder {
public:
    using Iterator = OutIt;
private:
    using Seq = Sequence<OutIt, OutItEnd>;

public:
    explicit BitEncoder(OutIt out_it) requires (not Seq::bounded)
        : seq(out_it)
    {
    }

    explicit BitEncoder(OutIt out_it, OutIt out_it_end) requires (Seq::bounded)
        : seq(out_it, out_it_end)
    {
    }

    template<std::output_iterator<Char> PrevOutIt>
    explicit BitEncoder(const BitEncoder<Char, char_size, PrevOutIt>& prev, OutIt out_it)
        requires (not Seq::bounded)
        : mid_off(prev.mid_off), mid_chr(prev.mid_chr), seq(out_it)
    {
    }

    template<std::output_iterator<Char> PrevOutIt>
    explicit BitEncoder(const BitEncoder<Char, char_size, PrevOutIt, PrevOutIt>& prev,
            OutIt out_it, OutIt out_it_end) requires (Seq::bounded)
        : mid_off(prev.mid_off), mid_chr(prev.mid_off), seq(out_it, out_it_end)
    {
    }

    /* Encode the bitset. */
    template<std::size_t nr_bits>
    inline bool encode_bits(const std::bitset<nr_bits>& bits)
    {
        return encode_bits(bits, nr_bits);
    }

    /* Encode the bitset with a custom number of bits. */
    template<std::size_t max_nr_bits>
    bool encode_bits(const std::bitset<max_nr_bits>& bits, std::size_t nr_bits)
    {
        encode_mid_bits(bits, nr_bits);
        if constexpr (char_size < max_nr_bits)
            encode_main_bits(bits, nr_bits);
        encode_remainder_bits(bits, nr_bits);
        return 0 == nr_bits;
    }

    /* Zero-pad remaining bits and flush them. */
    inline std::size_t finalize()
    {
        if (seq.is_valid() and char_size != mid_off) {
            *seq.it = mid_chr << mid_off; ++seq.it;
            reset();
        }
        return char_size - mid_off;
    }

    /* Reset the bit-encoder state. */
    inline void reset()
    {
        mid_chr = Char{};
        mid_off = char_size;
    }

    /* Get the current iterator. */
    inline OutIt get_it() const
    {
        return seq.it;
    }

    /* Check if there's space to write bits to. */
    inline bool is_valid() const noexcept
    {
        return seq.is_valid();
    }

    /* Continue by using a different unbounded iterator. */
    template<std::output_iterator<Char> NextOutIt>
    inline auto continue_by(NextOutIt it) const
    {
        return BitEncoder<Char, char_size, NextOutIt>(*this, it);
    }

    /* Continue by using a different bounded iterator. */
    template<std::output_iterator<Char> NextOutIt>
    inline auto continue_by(NextOutIt it, NextOutIt it_end) const
    {
        return BitEncoder<Char, char_size, NextOutIt, NextOutIt>(*this, it, it_end);
    }

    /* Find number of characters required to write for encoding the given amount of bits. */
    inline std::size_t find_chars_required_for(std::size_t nr_bits) const
    {
        return (nr_bits + char_size - mid_off) / char_size;
    }

private:
    /* Encode mid character bits. If mid offset becomes 0, flush and return. */
    template<typename Bitset>
    inline void encode_mid_bits(const Bitset& bits, std::size_t& nr_bits)
    {
        const std::size_t nr_mid_bits = std::min(nr_bits, mid_off);

        mid_chr = (mid_chr << nr_mid_bits) | (bits >> (nr_bits - nr_mid_bits)).to_ullong();
        mid_off -= nr_mid_bits;
        nr_bits -= nr_mid_bits;

        if (0 == mid_off) {
            mid_off = char_size;
            if (not seq.is_valid())
                return;
            *seq.it = mid_chr; ++seq.it;
        }
    }

    /* Encode main bits flushing them character-wise. */
    template<typename Bitset>
    inline void encode_main_bits(const Bitset& bits, std::size_t& nr_bits)
    {
        for (; seq.is_valid() && nr_bits >= char_size; ++seq.it, nr_bits -= char_size)
            *seq.it = static_cast<Char>((bits >> (nr_bits - char_size)).to_ullong()) & Char(-1);
    }

    /* Encode remainder bits left after previous two operations. These won't be flushed immediately
     * and wait for upcoming bits to combine with and flush together. */
    template<typename Bitset>
    inline void encode_remainder_bits(const Bitset& bits, std::size_t& nr_bits)
    {
        assert(nr_bits < char_size);
        mid_chr = (mid_chr << nr_bits) | (static_cast<Char>(bits.to_ullong()) & ((Char(1) << nr_bits) - 1));
        mid_off -= nr_bits;
        nr_bits = 0;
    }

    /* Stores the not yet fully encoded character. */
    Char mid_chr {};
    /* Stores the number of bits left to write to get a fully encoded character.
     * Can also be thought of as a position of the unwritten bit stream within the mid char.
     * It's the opposite of Decoder::mid_pos and its initial value is char_size. */
    std::size_t mid_off = char_size;
    /* Output character sequence. */
    Seq seq;
};

/* This is a class used for decoding bits. Internally handles all the necessary bit operations
 * to provide a bit-reading medium interface. Reads bits from an input sequence defined by the
 * provided input iterator(s) */
template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
        std::input_iterator InIt = typename std::vector<Char>::iterator, typename InItEnd = void>
    requires (sizeof(Char) <= sizeof(unsigned long long) and
              (std::same_as<InItEnd, void> or std::same_as<InItEnd, InIt>))
class BitDecoder {
public:
    using Iterator = InIt;
private:
    using UChar = std::make_unsigned_t<Char>;
    using Seq = Sequence<InIt, InItEnd>;

public:
    explicit BitDecoder(InIt in_it) requires (not Seq::bounded) : seq(in_it)
    {
    }

    explicit BitDecoder(InIt in_it, InIt in_it_end) requires (Seq::bounded) : seq(in_it, in_it_end)
    {
    }

    template<std::input_iterator PrevInIt>
    explicit BitDecoder(const BitDecoder<Char, char_size, PrevInIt>& prev, InIt in_it)
        requires (not Seq::bounded)
        : mid_chr(prev.mid_chr), mid_pos(prev.mid_pos), seq(in_it)
    {
    }

    template<std::input_iterator PrevInIt>
    explicit BitDecoder(const BitDecoder<Char, char_size, PrevInIt, PrevInIt>& prev,
            InIt in_it, InIt in_it_end)
        requires (Seq::bounded)
        : mid_chr(prev.mid_chr), mid_pos(prev.mid_pos), seq(in_it, in_it_end)
    {
    }

    /* Decode the bitset. */
    template<std::size_t nr_bits>
    bool decode_bits(std::bitset<nr_bits>& bits)
    {
        return decode_bits(bits, nr_bits);
    }

    /* Decode the bitset with a custom number of bits. */
    template<std::size_t max_nr_bits>
    bool decode_bits(std::bitset<max_nr_bits>& bits, std::size_t nr_bits)
    {
        decode_mid_bits(bits, nr_bits);
        if constexpr (char_size < max_nr_bits)
            decode_main_bits(bits, nr_bits);
        if (0 != nr_bits)
            decode_remainder_bits(bits, nr_bits);
        return 0 == nr_bits;
    }

    /* Read a single bit. */
    bool read_bit()
    {
        if (0 == mid_pos) {
            if (not seq.is_valid())
                return false;

            mid_chr = *seq.it; ++seq.it;
            mid_pos = char_size;
        }

        --mid_pos;
        const bool bit = !!(mid_chr >> mid_pos);
        mid_chr &= (Char(1) << mid_pos) - 1;
        return bit;
    }

    /* Obtain the remaining bits and reset. */
    template<std::size_t max_nr_bits>
    void finalize(std::bitset<max_nr_bits>& bits, std::size_t nr_bits = max_nr_bits)
    {
        bits <<= mid_pos;
        bits |= std::bitset<max_nr_bits>(mid_chr);
        reset();
    }

    /* Reset the bit-decoder state. */
    inline void reset()
    {
        mid_chr = Char{};
        mid_pos = 0;
    }

    /* Get the current iterator. */
    inline InIt get_it() const
    {
        return seq.it;
    }

    /* Check if there's space to read bits from. */
    inline bool is_valid() const noexcept
    {
        return seq.is_valid() || 0 != mid_pos;
    }

    /* Continue by using a different unbounded iterator. */
    template<std::input_iterator NextInIt>
    inline auto continue_by(NextInIt next_it) const
    {
        return BitDecoder<Char, char_size, NextInIt>(*this, next_it);
    }

    /* Continue by using a different bounded iterator. */
    template<std::input_iterator NextInIt>
    inline auto continue_by(NextInIt next_it, NextInIt next_it_end) const
    {
        return BitDecoder<Char, char_size, NextInIt, NextInIt>(*this, next_it, next_it_end);
    }

    /* Find number of characters required to write for decoding the given amount of bits. */
    inline std::size_t find_chars_required_for(std::size_t nr_bits) const
    {
        const std::size_t N = (nr_bits + char_size - mid_pos);
        return (N / char_size) - !(N % char_size);
    }

    /* Make a bit-reader iterator. */
    auto make_bit_reader_iterator()
    {
        struct ReadBitFunctor {
            BitDecoder *self;

            inline bool operator()() const
            {
                return self->read_bit();
            }
        };
        return utils::FunctionInputIterator(ReadBitFunctor{this});
    }

private:
    /* Decode mid character bits. */
    template<typename Bitset>
    inline void decode_mid_bits(Bitset& bits, std::size_t& nr_bits)
    {
        const std::size_t nr_mid_bits = std::min(nr_bits, mid_pos);

        mid_pos -= nr_mid_bits;
        bits <<= nr_mid_bits;
        bits |= Bitset(static_cast<UChar>(mid_chr) >> mid_pos);
        mid_chr &= (Char(1) << mid_pos) - 1;
        nr_bits -= nr_mid_bits;
    }

    /* Decode main bits fetching them character-wise. */
    template<typename Bitset>
    inline void decode_main_bits(Bitset& bits, std::size_t& nr_bits)
    {
        for (; seq.is_valid() && nr_bits >= char_size; ++seq.it, nr_bits -= char_size) {
            bits <<= char_size;
            bits |= Bitset(*seq.it);
        }
    }

    /* Decode remainder bits that won't fully consume the fetched character.
     * Store the unused bits in the mid character to be processed later. */
    template<typename Bitset>
    inline void decode_remainder_bits(Bitset& bits, std::size_t& nr_bits)
    {
        assert(nr_bits < char_size);
        assert(nr_bits != 0);

        if (not seq.is_valid())
            return;

        mid_chr = *seq.it; ++seq.it;

        mid_pos = char_size - nr_bits;
        bits <<= nr_bits;
        bits |= Bitset(static_cast<UChar>(mid_chr) >> mid_pos);
        mid_chr &= (Char(1) << mid_pos) - 1;
        nr_bits = 0;
    }

    /* Stores the not yet fully decoded character. */
    Char mid_chr {};
    /* Stores the number of bits left to read to get a fully decoded character.
     * Can also be thought of as a position of the unread bit stream within the mid char.
     * It's the opposite of BitEncoder::mid_off and its initial value is 0. */
    std::size_t mid_pos = 0;
    /* Input character sequence. */
    Seq seq;
};

/* Make a bit encoder using unbounded output iterator. */
template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
    std::output_iterator<Char> OutIt>
auto make_bit_encoder(OutIt out_it)
{
    return BitEncoder<Char, char_size, OutIt, void>(out_it);
}

/* Make a bit encoder using bounded output iterator. */
template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
    std::output_iterator<Char> OutIt>
auto make_bit_encoder(OutIt out_it, OutIt out_it_end)
{
    return BitEncoder<Char, char_size, OutIt, OutIt>(out_it, out_it_end);
}

/* Make a bit decoder using unbounded output iterator. */
template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
    std::input_iterator InIt>
auto make_bit_decoder(InIt in_it)
{
    return BitDecoder<Char, char_size, InIt, void>(in_it);
}

/* Make a bit decoder using bounded output iterator. */
template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
    std::input_iterator InIt>
auto make_bit_decoder(InIt in_it, InIt in_it_end)
{
    return BitDecoder<Char, char_size, InIt, InIt>(in_it, in_it_end);
}

}
