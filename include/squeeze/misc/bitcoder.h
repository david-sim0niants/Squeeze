#pragma once

#include <cassert>
#include <cstdint>
#include <climits>
#include <iterator>
#include <bitset>
#include <utility>

namespace squeeze::misc {

template<typename Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT>
    requires (sizeof(Char) <= sizeof(unsigned long long))
class BitEncoder {
public:
    template<std::size_t nr_bits, std::output_iterator<Char> OutIt>
    inline auto encode_bits(const std::bitset<nr_bits>& bits, OutIt out_it, OutIt out_it_end)
    {
        return encode_bits(bits, nr_bits, out_it, out_it_end);
    }

    template<std::size_t nr_bits, std::output_iterator<Char> OutIt>
    inline auto encode_bits(const std::bitset<nr_bits>& bits, OutIt out_it)
    {
        return encode_bits(bits, nr_bits, out_it);
    }

    template<std::size_t max_nr_bits, std::output_iterator<Char> OutIt>
    std::pair<std::size_t, OutIt> encode_bits(
            const std::bitset<max_nr_bits>& bits, std::size_t nr_bits,
            OutIt out_it, OutIt out_it_end)
    {
        if (out_it == out_it_end)
            return std::make_pair(nr_bits, out_it);

        out_it = encode_mid_bits(bits, nr_bits, out_it);
        out_it = encode_main_bits(bits, nr_bits, out_it, out_it_end);

        if (nr_bits >= char_size)
            return std::make_pair(nr_bits, out_it);

        encode_remainder_bits(bits, nr_bits);
        return std::make_pair(0, out_it);
    }

    template<std::size_t max_nr_bits, std::output_iterator<Char> OutIt>
    OutIt encode_bits(const std::bitset<max_nr_bits>& bits, std::size_t nr_bits, OutIt out_it)
    {
        out_it = encode_mid_bits(bits, nr_bits, out_it);
        out_it = encode_main_bits(bits, nr_bits, out_it);
        encode_remainder_bits(bits, nr_bits);
        return out_it;
    }

    inline void finalize(Char& out)
    {
        out = mid_chr << mid_off;
        reset();
    }

    inline void reset()
    {
        mid_chr = Char{};
        mid_off = char_size;
    }

    inline std::size_t find_chars_required_for(std::size_t nr_bits) const
    {
        return (nr_bits + char_size - mid_off) / char_size;
    }

private:
    template<typename Bitset, std::output_iterator<Char> OutIt>
    inline OutIt encode_mid_bits(const Bitset& bits, std::size_t& nr_bits, OutIt out_it)
    {
        const std::size_t nr_mid_bits = std::min(nr_bits, mid_off);

        mid_chr = (mid_chr << nr_mid_bits) | (bits >> (nr_bits - nr_mid_bits)).to_ullong();
        mid_off -= nr_mid_bits;
        nr_bits -= nr_mid_bits;

        if (0 == mid_off) {
            mid_off = char_size;
            *out_it = mid_chr;
            ++out_it;
        }

        return out_it;
    }

    template<typename Bitset, std::output_iterator<Char> OutIt>
    inline OutIt encode_main_bits(const Bitset& bits, std::size_t& nr_bits, OutIt out_it)
    {
        for (; nr_bits >= char_size; ++out_it) {
            *out_it = static_cast<Char>((bits >> (nr_bits - char_size)).to_ullong()) & Char(-1);
            nr_bits -= char_size;
        }
        return out_it;
    }

    template<typename Bitset, std::output_iterator<Char> OutIt>
    inline OutIt encode_main_bits(const Bitset& bits, std::size_t& nr_bits,
            OutIt& out_it, OutIt out_it_end)
    {
        for (; out_it != out_it_end && nr_bits >= char_size; ++out_it) {
            *out_it = static_cast<Char>((bits >> (nr_bits - char_size)).to_ullong()) & Char(-1);
            nr_bits -= char_size;
        }
        return out_it;
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
};

template<std::integral Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT>
    requires (sizeof(Char) <= sizeof(unsigned long long))
class BitDecoder {
private:
    using UChar = std::make_unsigned_t<Char>;

public:
    template<std::input_iterator InIt, std::size_t nr_bits>
    auto decode_bits(InIt in_it, InIt in_it_end, std::bitset<nr_bits>& bits)
    {
        return decode_bits(in_it, in_it_end, bits, nr_bits);
    }

    template<std::input_iterator InIt, std::size_t nr_bits>
    auto decode_bits(InIt in_it, std::bitset<nr_bits>& bits)
    {
        return decode_bits(in_it, bits, nr_bits);
    }

    template<std::input_iterator InIt, std::size_t max_nr_bits>
    std::pair<std::size_t, InIt> decode_bits(InIt in_it, InIt in_it_end,
            std::bitset<max_nr_bits>& bits, std::size_t nr_bits)
    {
        if (in_it == in_it_end)
            return std::make_pair(nr_bits, in_it);

        decode_mid_bits(bits, nr_bits);
        in_it = decode_main_bits(bits, nr_bits, in_it, in_it_end);

        if (0 == nr_bits || nr_bits >= char_size || in_it == in_it_end)
            return std::make_pair(nr_bits, in_it);

        return std::make_pair(0, decode_remainder_bits(bits, nr_bits, in_it));
    }

    template<std::input_iterator InIt, std::size_t max_nr_bits>
    InIt decode_bits(InIt in_it, std::bitset<max_nr_bits>& bits, std::size_t nr_bits)
    {
        decode_mid_bits(bits, nr_bits);
        in_it = decode_main_bits(bits, nr_bits, in_it);
        return nr_bits == 0 ? in_it : decode_remainder_bits(bits, nr_bits, in_it);
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

    template<typename Bitset, std::input_iterator InIt>
    inline InIt decode_main_bits(Bitset& bits, std::size_t& nr_bits, InIt in_it, InIt in_it_end)
    {
        for (; in_it != in_it_end && nr_bits >= char_size; ++in_it) {
            bits <<= char_size;
            bits |= Bitset(*in_it);
            nr_bits -= char_size;
        }
        return in_it;
    }

    template<typename Bitset, std::input_iterator InIt>
    inline InIt decode_main_bits(Bitset& bits, std::size_t& nr_bits, InIt in_it)
    {
        for (; nr_bits >= char_size; ++in_it) {
            bits <<= char_size;
            bits |= Bitset(*in_it);
            nr_bits -= char_size;
        }
        return in_it;
    }

    template<typename Bitset, std::input_iterator InIt>
    inline InIt decode_remainder_bits(Bitset& bits, std::size_t nr_bits, InIt in_it)
    {
        assert(nr_bits < char_size);
        assert(nr_bits != 0);

        mid_chr = *in_it; ++in_it;

        mid_pos = char_size - nr_bits;
        bits <<= nr_bits;
        bits |= Bitset(static_cast<UChar>(mid_chr) >> mid_pos);
        mid_chr &= (Char(1) << mid_pos) - 1;

        return in_it;
    }

    Char mid_chr {};
    std::size_t mid_pos = 0;
};

template<std::integral Char = char, std::size_t char_size = sizeof(Char) * CHAR_BIT>
    requires (sizeof(Char) <= sizeof(unsigned long long))
class BitCoder : public BitEncoder<Char, char_size>, public BitDecoder<Char, char_size> {};

}
