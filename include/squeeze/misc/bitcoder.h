#pragma once

#include <cassert>
#include <cstdint>
#include <climits>
#include <iterator>
#include <bitset>
#include <utility>
#include <vector>

namespace squeeze::misc {

namespace detail {

template<typename It, typename ItEnd>
struct IteratorHandle;

template<typename It> struct IteratorHandle<It, void> {
    explicit IteratorHandle(It it) : it(it)
    {
    }

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

    inline bool is_valid() const
    {
        return it != it_end;
    }

    static constexpr bool bounded = true;

    It it, it_end;
};

}

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
    explicit BitEncoder(const BitEncoder<Char, char_size, PrevOutIt>& prev,
            OutIt out_it, OutIt out_it_end) requires (IteratorHandle::bounded)
        : mid_off(prev.mid_off), mid_chr(prev.mid_off), it_handle(out_it, out_it_end)
    {
    }

    template<std::size_t nr_bits>
    inline std::size_t encode_bits(const std::bitset<nr_bits>& bits)
    {
        return encode_bits(bits, nr_bits);
    }

    template<std::size_t max_nr_bits>
    std::size_t encode_bits(const std::bitset<max_nr_bits>& bits, std::size_t nr_bits)
    {
        encode_mid_bits(bits, nr_bits);
        encode_main_bits(bits, nr_bits);
        encode_remainder_bits(bits, nr_bits);
        return nr_bits;
    }

    inline std::size_t finalize()
    {
        if (it_handle.is_valid() and char_size != mid_off) {
            *it_handle.it = mid_chr << mid_off; ++it_handle.it;
            reset();
        }
        return char_size - mid_off;
    }

    inline void reset()
    {
        mid_chr = Char{};
        mid_off = char_size;
    }

    inline OutIt get_it() const
    {
        return it_handle.it;
    }

    template<std::output_iterator<Char> NextOutIt>
    inline auto continue_by(NextOutIt it) const
    {
        return BitEncoder<Char, char_size>(*this, it);
    }

    template<std::output_iterator<Char> NextOutIt>
    inline auto continue_by(NextOutIt it, NextOutIt it_end) const
    {
        return BitEncoder<Char, char_size>(*this, it, it_end);
    }

    inline std::size_t find_chars_required_for(std::size_t nr_bits) const
    {
        return (nr_bits + char_size - mid_off) / char_size;
    }

private:
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

    template<typename Bitset>
    inline void encode_main_bits(const Bitset& bits, std::size_t& nr_bits)
    {
        for (; it_handle.is_valid() && nr_bits >= char_size; ++it_handle.it, nr_bits -= char_size)
            *it_handle.it = static_cast<Char>((bits >> (nr_bits - char_size)).to_ullong()) & Char(-1);
    }

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
    explicit BitDecoder(const BitDecoder<Char, char_size, PrevInIt>& prev, InIt in_it, InIt in_it_end)
        requires (IteratorHandle::bounded)
        : mid_chr(prev.mid_chr), mid_pos(prev.mid_pos), it_handle(in_it, in_it_end)
    {
    }

    template<std::size_t nr_bits>
    std::size_t decode_bits(std::bitset<nr_bits>& bits)
    {
        return decode_bits(bits, nr_bits);
    }

    template<std::size_t max_nr_bits>
    std::size_t decode_bits(std::bitset<max_nr_bits>& bits, std::size_t nr_bits)
    {
        decode_mid_bits(bits, nr_bits);
        decode_main_bits(bits, nr_bits);
        if (0 != nr_bits)
            decode_remainder_bits(bits, nr_bits);
        return nr_bits;
    }

    template<std::size_t max_nr_bits>
    void finalize(std::bitset<max_nr_bits>& bits, std::size_t nr_bits = max_nr_bits)
    {
        bits <<= mid_pos;
        bits |= std::bitset<max_nr_bits>(mid_chr);
        reset();
    }

    inline void reset()
    {
        mid_chr = Char{};
        mid_pos = 0;
    }

    template<std::input_iterator NextInIt>
    inline auto continue_by(NextInIt next_it) const
    {
        return BitDecoder<Char, char_size>(*this, next_it);
    }

    template<std::input_iterator NextInIt>
    inline auto continue_by(NextInIt next_it, NextInIt next_it_end) const
    {
        return BitDecoder<Char, char_size>(*this, next_it, next_it_end);
    }

    inline std::size_t find_chars_required_for(std::size_t nr_bits) const
    {
        const std::size_t N = (nr_bits + char_size - mid_pos);
        return (N / char_size) - !(N % char_size);
    }

private:
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

    template<typename Bitset>
    inline void decode_main_bits(Bitset& bits, std::size_t& nr_bits)
    {
        for (; it_handle.is_valid() && nr_bits >= char_size; ++it_handle.it, nr_bits -= char_size) {
            bits <<= char_size;
            bits |= Bitset(*it_handle.it);
        }
    }

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

template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
    std::output_iterator<Char> OutIt>
auto make_bit_encoder(OutIt out_it)
{
    return BitEncoder<Char, char_size, OutIt, void>(out_it);
}

template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
    std::output_iterator<Char> OutIt>
auto make_bit_encoder(OutIt out_it, OutIt out_it_end)
{
    return BitEncoder<Char, char_size, OutIt, OutIt>(out_it, out_it_end);
}

template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
    std::input_iterator InIt>
auto make_bit_decoder(InIt in_it)
{
    return BitDecoder<Char, char_size, InIt, void>(in_it);
}

template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT,
    std::input_iterator InIt>
auto make_bit_decoder(InIt in_it, InIt in_it_end)
{
    return BitDecoder<Char, char_size, InIt, InIt>(in_it, in_it_end);
}

}
