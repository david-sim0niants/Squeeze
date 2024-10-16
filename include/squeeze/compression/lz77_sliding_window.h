// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <algorithm>
#include <array>

#include "squeeze/misc/sequence.h"
#include "squeeze/misc/circular_iterator.h"

namespace squeeze::compression {

/* Sliding window utility for LZ77 encoding/decoding implemented as a circular buffer.
 * Stores a static buffer and a circularly iterating pivot point that points both
 * to the start and end of the circular buffer. Maintains an absolute position of the start of the buffer. */
template<typename Sym, std::size_t size>
class LZ77SlidingWindow {
public:
    using CircularIterator = misc::CircularIterator<Sym, size>;

    LZ77SlidingWindow() : pivot(buffer.data())
    {
    }

    inline void push_sym(Sym sym)
    {
        *pivot = sym;
        ++pivot;
        ++pos;
    }

    inline CircularIterator& get_pivot() noexcept
    {
        return pivot;
    }

    inline const CircularIterator& get_pivot() const noexcept
    {
        return pivot;
    }

    inline CircularIterator get_iter_at(std::size_t pos) const
    {
        return pivot + (pos - this->pos);
    }

    inline auto& get_buffer() noexcept
    {
        return buffer;
    }

    inline const auto& get_buffer() const noexcept
    {
        return buffer;
    }

    inline std::size_t get_pos() const noexcept
    {
        return pos;
    }

    inline std::size_t get_end_pos() const noexcept
    {
        return pos + size;
    }

    inline std::size_t get_filled_size() const noexcept
    {
        return std::min(size, pos);
    }

    inline std::size_t get_filled_pos() const noexcept
    {
        return std::max(pos, size);
    }

    /* Check if the suffix of the given length matches a sequence of the same length
     * at the specified position. */
    bool suffix_matches_at(std::size_t match_pos, std::size_t suffix_len) const
    {
        assert(match_pos >= pos);
        const std::size_t match_rel_pos = match_pos - pos;
        return std::mismatch(pivot - suffix_len, pivot, pivot + match_rel_pos).first == pivot;
    }

private:
    std::array<Sym, size> buffer {};
    CircularIterator pivot;
    std::size_t pos = 0;
};

}
