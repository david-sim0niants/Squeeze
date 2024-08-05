#pragma once

#include <cassert>
#include <climits>
#include <bitset>
#include <istream>
#include <ostream>
#include <vector>

#include "squeeze/error.h"

namespace squeeze::misc {

class BitStreamReader {
public:
    explicit BitStreamReader(std::istream& input) : input(input)
    {
    }

    template<std::size_t max_nr_bits>
    Error<BitStreamReader> read(std::bitset<max_nr_bits>& bits, std::size_t nr_bits = max_nr_bits)
    {
        nr_bits = read_rsplit(bits, nr_bits);

        if (not read_from_stream(nr_bits))
            return {"failed reading from stream", ErrorCode::from_current_errno().report()};

        std::size_t i_byte = 0;
        while (nr_bits >= CHAR_BIT) {
            bits <<= CHAR_BIT;
            bits |= std::bitset<max_nr_bits>(buffer[i_byte]);
            nr_bits -= CHAR_BIT;
            ++i_byte;
        }

        nr_bits = read_lsplit(bits, nr_bits);
        return success;
    }

private:
    inline bool read_from_stream(std::size_t nr_bits)
    {
        const std::size_t nr_bytes = nr_bits / CHAR_BIT + !!(nr_bits % CHAR_BIT);
        buffer.resize(nr_bytes);
        input.read(buffer.data(), nr_bytes);
        return not input.fail();
    }

    template<typename Bitset>
    std::size_t read_rsplit(Bitset& bits, std::size_t nr_bits)
    {
        assert(mid_pos < CHAR_BIT);
        const unsigned char rsplit_len = (CHAR_BIT - mid_pos) % CHAR_BIT;
        const std::size_t nr_mid_bits = std::min(nr_bits, std::size_t(rsplit_len));

        bits <<= nr_mid_bits;
        bits |= Bitset((mid_byte >> (rsplit_len - nr_mid_bits)) & ((1 << nr_mid_bits) - 1));
        mid_pos = (mid_pos + nr_mid_bits) % CHAR_BIT;
        return nr_bits - nr_mid_bits;
    }

    template<typename Bitset>
    std::size_t read_lsplit(Bitset& bits, std::size_t nr_bits)
    {
        assert(mid_pos < CHAR_BIT);
        assert(nr_bits < CHAR_BIT);

        if (nr_bits == 0)
            return 0;

        mid_byte = buffer.back();
        bits <<= nr_bits;
        bits |= Bitset((mid_byte >> (CHAR_BIT - nr_bits)) | ((1 << nr_bits) - 1));
        mid_pos = (mid_pos + nr_bits) % CHAR_BIT;
        return 0;
    }

    std::istream& input;
    char mid_byte = 0;
    unsigned char mid_pos = 0;
    std::vector<char> buffer;
};

class BitStreamWriter {
public:
    explicit BitStreamWriter(std::ostream& output) : output(output)
    {
    }

    template<std::size_t max_nr_bits>
    Error<BitStreamWriter> write(const std::bitset<max_nr_bits>& bits, std::size_t nr_bits = max_nr_bits)
    {
        reset_buffer(nr_bits);

        nr_bits = write_rsplit(bits, nr_bits);

        while (nr_bits >= CHAR_BIT) {
            buffer.push_back(static_cast<char>((bits >> nr_bits).to_ulong()));
            nr_bits -= CHAR_BIT;
        }

        nr_bits = write_lsplit(bits, nr_bits);

        if (not write_to_stream())
            return {"failed writing to stream", ErrorCode::from_current_errno().report()};

        return success;
    }

    inline bool finalize(bool bit_fill = false)
    {
        if (mid_pos == 0)
            return true;
        output.put((mid_byte << mid_pos) | (bit_fill ? ~(1 << mid_pos) : 0));
        return not output.fail();
    }

private:
    inline void reset_buffer(std::size_t nr_bits)
    {
        const std::size_t nr_bytes = (mid_pos + nr_bits) / CHAR_BIT;
        buffer.resize(0);
        buffer.reserve(nr_bytes);
    }

    inline bool write_to_stream()
    {
        output.write(buffer.data(), buffer.size());
        return not output.fail();
    }

    template<typename Bitset>
    std::size_t write_rsplit(const Bitset& bits, std::size_t nr_bits)
    {
        assert(mid_pos < CHAR_BIT);
        const unsigned char rsplit_len = (CHAR_BIT - mid_pos) % CHAR_BIT;
        const std::size_t nr_mid_bits = std::min(nr_bits, std::size_t(rsplit_len));

        const char new_mid_byte = static_cast<char>(
                (mid_byte << nr_mid_bits) |
                (bits >> (nr_bits - nr_mid_bits)).to_ulong() & ((1 << nr_mid_bits) - 1) );

        if (nr_mid_bits == rsplit_len)
            buffer.push_back(mid_byte);

        mid_byte = new_mid_byte;
        mid_pos = (mid_pos + nr_mid_bits) % CHAR_BIT;
        return nr_bits - nr_mid_bits;
    }

    template<typename Bitset>
    std::size_t write_lsplit(const Bitset& bits, std::size_t nr_bits)
    {
        assert(mid_pos < CHAR_BIT);
        assert(nr_bits < CHAR_BIT);

        mid_byte = static_cast<char>(bits.to_ulong());
        mid_pos = (mid_pos + nr_bits) % CHAR_BIT;
        return 0;
    }

    std::ostream& output;
    char mid_byte = 0;
    unsigned char mid_pos = 0;
    std::vector<char> buffer;
};

}

