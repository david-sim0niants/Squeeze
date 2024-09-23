#pragma once

#include <concepts>
#include <iterator>
#include <cassert>
#include <vector>
#include <tuple>
#include <algorithm>

#include "squeeze/misc/sequence.h"
#include "squeeze/misc/circular_iterator.h"

namespace squeeze::compression {

/* Sliding window utility for LZ77. Supports peeking/advancing and finding repeated sequence matches.
 * Handles absolute positions by keeping the offset of its search window.
 * Consists of two consecutive sliding windows - search and lookahead.
 * Advancing pushes symbols from the lookahead window front to the search window back. */
template<typename Sym, std::input_iterator InIt, typename... InItEnd>
class LZ77SlidingWindow {
public:
    using CircularIterator = misc::CircularIterator<Sym>;

    struct Pivot {
        CircularIterator iter;
        std::size_t pos;

        Pivot(Sym *data, std::size_t size, std::size_t pos) : iter(data, size), pos(pos)
        {
        }
    };

    /* Construct from the provided search_size, lookahead_size and input iterator(s); */
    LZ77SlidingWindow(std::size_t search_size, std::size_t lookahead_size, InIt in_it, InItEnd... in_it_end)
        :
        search_size(search_size), lookahead_size(lookahead_size),
        buffer(search_size + lookahead_size), pivot(buffer.data(), buffer.size(), search_size),
        seq(in_it, in_it_end...)
    {
        fetch_data(lookahead_size);
    }

    /* Construct as a continuation of another sliding window with new iterator(s). */
    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    LZ77SlidingWindow(const LZ77SlidingWindow<PrevInIt, PrevInItEnd...>& prev, InIt in_it, InItEnd... in_it_end)
        :
        search_size(prev.search_size), lookahead_size(prev.lookahead_size),
        buffer(buffer), pivot(prev.pivot),
        seq(in_it, in_it_end...)
    {
    }

    /* Construct as a continuation of another sliding window with new iterator(s). */
    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    LZ77SlidingWindow(LZ77SlidingWindow<PrevInIt, PrevInItEnd...>&& prev, InIt in_it, InItEnd... in_it_end)
        :
        search_size(prev.search_size), lookahead_size(prev.lookahead_size),
        buffer(std::move(buffer)), pivot(std::move(prev.pivot)),
        seq(in_it, in_it_end...)
    {
    }

    /* Check if the window is empty and no more data is to come. */
    inline bool is_empty() const noexcept
    {
        return !seq.is_valid() && 0 == lookahead_size;
    }

    /* Get the lookahead size. */
    inline std::size_t get_lookahead_size() const noexcept
    {
        return lookahead_size;
    }

    /* Get the search size. */
    inline std::size_t get_search_size() const noexcept
    {
        return search_size;
    }

    /* Get the current absolute search buffer position (the position of the first symbol of it). */
    inline std::size_t get_search_pos() const noexcept
    {
        return pivot.pos - search_size;
    }

    /* Get the current absolute lookahead buffer position (the position of the first symbol of it). */
    inline std::size_t get_lookahead_pos() const noexcept
    {
        return pivot.pos;
    }

    /* Peek at the lookahead window. Returns the iterator at which the peek stopped. */
    template<std::output_iterator<Sym> OutIt>
    OutIt peek(OutIt peek_it, std::size_t n)
    {
        return std::copy_n(pivot.iter, std::min(n, lookahead_size), peek_it);
    }

    /* Advance by one symbol. */
    void advance_once()
    {
        assert(1 <= lookahead_size);
        ++pivot.iter;
        ++pivot.pos;
        fetch_data(1);
    }

    /* Advance by the given length of symbols. The given length shall not exceed a match length
     * found by a prior call to find_match() and the overall lookahead size. */
    void advance(std::size_t len)
    {
        assert(len <= lookahead_size);
        pivot.iter += len;
        pivot.pos += len;
        fetch_data(len);
    }

    /* Find the longest match starting from the given position. */
    std::pair<std::size_t, std::size_t> find_match(std::size_t pos) const
    {
        assert(pos < pivot.pos);
        assert(pivot.pos - pos <= search_size);

        CircularIterator lookah_it = pivot.iter;
        CircularIterator lookah_it_end = lookah_it + lookahead_size;
        CircularIterator search_it = pivot.iter - (pivot.pos - pos);

        std::tie(lookah_it, search_it) = std::mismatch(lookah_it, lookah_it_end, search_it);

        const std::size_t match_len = std::distance(pivot.iter, lookah_it);
        const std::size_t match_dist = pivot.pos - pos;
        return std::make_pair(match_len, match_dist);
    }

    /* Get current iterator. */
    inline InIt& get_it()
    {
        return seq.it;
    }

    /* Get current iterator. */
    inline const InIt& get_it() const
    {
        return seq.it;
    }

    /* Continue by using different iterator(s). */
    template<std::input_iterator NextIt, typename... NextInItEnd>
    inline auto continue_by(NextIt it, NextInItEnd... it_end) const &
    {
        return SlidingWindow<NextIt, NextInItEnd...>(*this, it, it_end...);
    }

    /* Continue by using different iterator(s). */
    template<std::input_iterator NextIt, typename... NextInItEnd>
    inline auto continue_by(NextIt it, NextInItEnd... it_end) &&
    {
        return SlidingWindow<NextIt, NextInItEnd...>(std::move(*this), it, it_end...);
    }

private:
    inline void fetch_data(std::size_t len)
    {
        CircularIterator fill_it = pivot.iter + (lookahead_size - len);
        for (; seq.is_valid() && len > 0; ++seq.it, ++fill_it, --len)
            *fill_it = *seq.it;
        lookahead_size -= len;
    }

    std::size_t search_size, lookahead_size;
    std::vector<Sym> buffer;
    Pivot pivot;
    misc::Sequence<InIt, InItEnd...> seq;
};

}
