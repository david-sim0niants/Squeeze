#pragma once

#include <cassert>
#include <cstdint>
#include <climits>
#include <iterator>
#include <bitset>
#include <utility>
#include <vector>

#include "squeeze/utils/iterator.h"

namespace squeeze::misc {

namespace detail {

/* Abstraction for handling two types of iterators - bounded iterator
 * with its corresponding end iterator and an unbounded iterator. */
template<typename It, typename ItEnd>
struct IteratorHandle;

template<typename It> struct IteratorHandle<It, void> {
    explicit IteratorHandle(It it) : it(it)
    {
    }

    // unbounded iterator, always valid
    inline bool is_valid() const
    {
        return true;
    }

    static constexpr bool bounded = false;

    It it;
};

template<typename It> struct IteratorHandle<It, It> {
    explicit IteratorHandle(It it, It it_end) : it(it), it_end(it_end)
    {
    }

    // bounded iterator, valid as long as not equal to its end iterator
    inline bool is_valid() const
    {
        return it != it_end;
    }

    static constexpr bool bounded = true;

    It it, it_end;
};

}

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
    using IteratorHandle = detail::IteratorHandle<OutIt, OutItEnd>;

public:
    explicit BitEncoder(OutIt out_it) requires (not IteratorHandle::bounded)
        : it_handle(out_it)
    {
    }

    explicit BitEncoder(OutIt out_it, OutIt out_it_end) requires (IteratorHandle::bounded)
        : it_handle(out_it, out_it_end)
    {
    }

    template<std::output_iterator<Char> PrevOutIt>
    explicit BitEncoder(const BitEncoder<Char, char_size, PrevOutIt>& prev, OutIt out_it)
        requires (not IteratorHandle::bounded)
        : mid_off(prev.mid_off), mid_chr(prev.mid_chr), it_handle(out_it)
    {
    }

    template<std::output_iterator<Char> PrevOutIt>
    explicit BitEncoder(const BitEncoder<Char, char_size, PrevOutIt, PrevOutIt>& prev,
            OutIt out_it, OutIt out_it_end) requires (IteratorHandle::bounded)
        : mid_off(prev.mid_off), mid_chr(prev.mid_off), it_handle(out_it, out_it_end)
    {
    }

    /* Encode the bitset. */
    template<std::size_t nr_bits>
    inline std::size_t encode_bits(const std::bitset<nr_bits>& bits)
    {
        return encode_bits(bits, nr_bits);
    }

    /* Encode the bitset with a custom number of bits. */
    template<std::size_t max_nr_bits>
    std::size_t encode_bits(const std::bitset<max_nr_bits>& bits, std::size_t nr_bits)
    {
        encode_mid_bits(bits, nr_bits);
        if constexpr (max_nr_bits + 1 >= 2 * char_size)
            encode_main_bits(bits, nr_bits);
        encode_remainder_bits(bits, nr_bits);
        return nr_bits;
    }

    /* Zero-pad remaining bits and flush them. */
    inline std::size_t finalize()
    {
        if (it_handle.is_valid() and char_size != mid_off) {
            *it_handle.it = mid_chr << mid_off; ++it_handle.it;
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
        return it_handle.it;
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
            if (not it_handle.is_valid())
                return;
            *it_handle.it = mid_chr; ++it_handle.it;
        }
    }

    /* Encode main bits flushing them character-wise. */
    template<typename Bitset>
    inline void encode_main_bits(const Bitset& bits, std::size_t& nr_bits)
    {
        for (; it_handle.is_valid() && nr_bits >= char_size; ++it_handle.it, nr_bits -= char_size)
            *it_handle.it = static_cast<Char>((bits >> (nr_bits - char_size)).to_ullong()) & Char(-1);
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

    Char mid_chr {};
    std::size_t mid_off = char_size;
    IteratorHandle it_handle;
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
    using IteratorHandle = detail::IteratorHandle<InIt, InItEnd>;

public:
    explicit BitDecoder(InIt in_it) requires (not IteratorHandle::bounded)
        : it_handle(in_it)
    {
    }

    explicit BitDecoder(InIt in_it, InIt in_it_end) requires (IteratorHandle::bounded)
        : it_handle(in_it, in_it_end)
    {
    }

    template<std::input_iterator PrevInIt>
    explicit BitDecoder(const BitDecoder<Char, char_size, PrevInIt>& prev, InIt in_it)
        requires (not IteratorHandle::bounded)
        : mid_chr(prev.mid_chr), mid_pos(prev.mid_pos), it_handle(in_it)
    {
    }

    template<std::input_iterator PrevInIt>
    explicit BitDecoder(const BitDecoder<Char, char_size, PrevInIt, PrevInIt>& prev,
            InIt in_it, InIt in_it_end)
        requires (IteratorHandle::bounded)
        : mid_chr(prev.mid_chr), mid_pos(prev.mid_pos), it_handle(in_it, in_it_end)
    {
    }

    /* Decode the bitset. */
    template<std::size_t nr_bits>
    std::size_t decode_bits(std::bitset<nr_bits>& bits)
    {
        return decode_bits(bits, nr_bits);
    }

    /* Decode the bitset with a custom number of bits. */
    template<std::size_t max_nr_bits>
    std::size_t decode_bits(std::bitset<max_nr_bits>& bits, std::size_t nr_bits)
    {
        decode_mid_bits(bits, nr_bits);
        if constexpr (max_nr_bits + 1 >= 2 * char_size)
            decode_main_bits(bits, nr_bits);
        if (0 != nr_bits)
            decode_remainder_bits(bits, nr_bits);
        return nr_bits;
    }

    /* Read a single bit. */
    bool read_bit(std::size_t& nr_bits_unread)
    {
        if (0 == mid_pos) {
            if (not it_handle.is_valid()) {
                ++nr_bits_unread;
                return false;
            }

            mid_chr = *it_handle.it; ++it_handle.it;
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
        return it_handle.it;
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
    auto make_bit_reader_iterator(std::size_t& nr_bits_unread)
    {
        struct ReadBitFunctor {
            BitDecoder *self;
            std::size_t *nr_bits_unread;

            inline bool operator()() const
            {
                return self->read_bit(*nr_bits_unread);
            }
        };
        return utils::FunctionInputIterator(ReadBitFunctor{this, &nr_bits_unread});
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
        for (; it_handle.is_valid() && nr_bits >= char_size; ++it_handle.it, nr_bits -= char_size) {
            bits <<= char_size;
            bits |= Bitset(*it_handle.it);
        }
    }

    /* Decode remainder bits that won't fully consume the fetched character.
     * Store the unused bits in the mid character to be processed later. */
    template<typename Bitset>
    inline void decode_remainder_bits(Bitset& bits, std::size_t& nr_bits)
    {
        assert(nr_bits < char_size);
        assert(nr_bits != 0);

        if (not it_handle.is_valid())
            return;

        mid_chr = *it_handle.it; ++it_handle.it;

        mid_pos = char_size - nr_bits;
        bits <<= nr_bits;
        bits |= Bitset(static_cast<UChar>(mid_chr) >> mid_pos);
        mid_chr &= (Char(1) << mid_pos) - 1;
        nr_bits = 0;
    }

    Char mid_chr {};
    std::size_t mid_pos = 0;
    IteratorHandle it_handle;
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
