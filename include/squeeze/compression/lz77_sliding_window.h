#pragma once

#include <concepts>
#include <iterator>
#include <cassert>
#include <deque>
#include <tuple>

#include "squeeze/misc/sequence.h"

namespace squeeze::compression {

/* Sliding window utility for LZ77. Supports peeking/advancing and finding repeated sequence matches.
 * Handles absolute positions by keeping the offset of its search window.
 * Consists of two consecutive sliding windows - search and lookahead.
 * Advancing pushes symbols from the lookahead window front to the search window back. */
template<typename Sym, std::input_iterator InIt, typename... InItEnd>
class LZ77SlidingWindow {
public:
    /* Construct from the provided search_size, lookahead_size and input iterator(s); */
    LZ77SlidingWindow(std::size_t search_size, std::size_t lookahead_size, InIt in_it, InItEnd... in_it_end)
        : search_size(search_size), lookahead_size(lookahead_size), seq(in_it, in_it_end...)
    {
        refetch_data();
    }

    /* Construct as a continuation of another sliding window with new iterator(s). */
    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    LZ77SlidingWindow(const LZ77SlidingWindow<PrevInIt, PrevInItEnd...>& prev, InIt in_it, InItEnd... in_it_end)
        :
        search_size(prev.search_size), lookahead_size(prev.lookahead_size),
        search(prev.search), lookahead(prev.lookahead),
        offset(prev.offset),
        seq(in_it, in_it_end...)
    {
        refetch_data();
    }

    /* Construct as a continuation of another sliding window with new iterator(s). */
    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    LZ77SlidingWindow(LZ77SlidingWindow<PrevInIt, PrevInItEnd...>&& prev, InIt in_it, InItEnd... in_it_end)
        :
        search_size(prev.search_size), lookahead_size(prev.lookahead_size),
        search(std::move(prev.search)), lookahead(std::move(prev.lookahead)),
        offset(prev.offset),
        seq(in_it, in_it_end...)
    {
        refetch_data();
    }

    /* Check if the window is empty and no more data is to come. */
    inline bool is_empty() const noexcept
    {
        return !seq.is_valid() && lookahead.empty();
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

    /* Get the current search size. */
    inline std::size_t get_curr_search_size() const noexcept
    {
        return search.size();
    }

    /* Get the current offset. */
    inline std::size_t get_offset() const noexcept
    {
        return offset;
    }

    /* Get the absolute position of the first symbol in the lookahead window. */
    inline std::size_t get_curr_peek_pos() const noexcept
    {
        return offset + search.size();
    }

    /* Peek at the lookahead window. Returns the iterator at which the peek stopped. */
    template<std::output_iterator<Sym> OutIt>
    OutIt peek(OutIt peek_it, std::size_t n)
    {
        refetch_data();
        return std::copy_n(lookahead.begin(), std::min(n, lookahead.size()), peek_it);
    }

    /* Advance by one symbol. Assuming the lookahead window is not empty, and thus
     * must only be called after a prior call to peek() that returned non-empty data. */
    void advance_once()
    {
        assert(!lookahead.empty());
        search.push_back(lookahead.front());
        lookahead.pop_front();
        if (search.size() > search_size) {
            search.pop_front();
            ++offset;
        }
    }

    /* Advance by the given length of symbols. The given length shall not exceed a match length
     * found by a prior call to find_match() and the overall lookahead size. */
    void advance(std::size_t len)
    {
        assert(len <= lookahead.size());
        search.insert(search.end(),
                lookahead.begin(), lookahead.begin() + len);
        lookahead.erase(lookahead.begin(), lookahead.begin() + len);
        const std::size_t search_advance_len =
            search.size() - std::min(search.size(), search_size);
        search.erase(search.begin(), search.begin() + search_advance_len);
        offset += search_advance_len;
    }

    /* Find the longest match starting from the given position. Assuming pos >= offset. */
    std::pair<std::size_t, std::size_t> find_match(std::size_t pos) const
    {
        assert(pos >= offset);

        auto search_it = search.begin() + (pos - offset);
        auto lookah_it = lookahead.begin();
        const auto search_it_end = search.end();
        const auto lookah_it_end = lookahead.end();

        std::tie(search_it, lookah_it) = mismatch(search_it, search_it_end, lookah_it, lookah_it_end);
        if (search_it == search_it_end) // search overflow case
            // finding rest of the match within the lookahead window
            lookah_it = std::mismatch(lookah_it, lookah_it_end, lookahead.begin()).first;

        const std::size_t match_len = std::distance(lookahead.begin(), lookah_it);
        const std::size_t match_dist = search.size() - (pos - offset);
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
    /* Custom implementation of std::mismatch with addition of checking both iterator ends. */
    template<std::input_iterator ItL, std::input_iterator ItR>
    static std::pair<ItL, ItR> mismatch(ItL it_l, ItL it_l_end, ItR it_r, ItR it_r_end)
    {
        for (; it_l != it_l_end && it_r != it_r_end && *it_l == *it_r; ++it_l, ++it_r);
        return std::make_pair(it_l, it_r);
    }

    /* Re-fetch data from the iterator(s). */
    inline void refetch_data()
    {
        for (; seq.is_valid() && lookahead.size() < lookahead_size; ++seq.it)
            lookahead.push_back(*seq.it);
    }

    std::size_t search_size, lookahead_size;
    std::deque<Sym> search, lookahead;
    std::size_t offset = 0;
    misc::Sequence<InIt, InItEnd...> seq;
};

template<std::input_iterator InIt, typename... InItEnd>
LZ77SlidingWindow(InIt, InItEnd...) ->
    LZ77SlidingWindow<std::remove_cvref_t<std::iter_value_t<InIt>>, InIt, InItEnd...>;

}
