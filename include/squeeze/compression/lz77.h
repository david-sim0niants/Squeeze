#pragma once

#include <array>
#include <algorithm>
#include <cassert>
#include <climits>
#include <deque>
#include <unordered_map>
#include <bitset>
#include <forward_list>
#include <variant>

#include "lz77_policy.h"
#include "squeeze/exception.h"
#include "squeeze/error.h"
#include "squeeze/misc/sequence.h"
#include "squeeze/utils/overloaded.h"

namespace squeeze::compression {

template<LZ77Policy Policy = BasicLZ77Policy<>>
class LZ77 {
public:
    using Literal = typename Policy::Literal;
    using Len = typename Policy::Len;
    using Dist = typename Policy::Dist;
    static constexpr Len min_len = Policy::min_len;

    struct LenDist {
        Len len;
        Dist dist;
    };
    using Token = std::variant<std::monostate, Literal, LenDist>;

    template<std::input_iterator InIt, typename... InItEnd>
    class SlidingWindow;

    template<std::input_iterator InIt, typename... InItEnd>
    class Encoder;
    template<std::output_iterator<typename Policy::Literal> OutIt, typename... OutItEnd>
    class Decoder;

    template<std::input_iterator InIt, typename... InItEnd>
    static inline Encoder<InIt, InItEnd...> make_encoder(
            std::size_t search_size, std::size_t lookahead_size, InIt in_it, InItEnd... in_it_end)
    {
        return {search_size, lookahead_size, in_it, in_it_end...};
    }

    template<std::output_iterator<Literal> OutIt, typename... OutItEnd>
    static inline Decoder<OutIt, OutItEnd...> make_decoder(
            std::size_t search_size, std::size_t lookahead_size, OutIt out_it, OutItEnd... out_it_end)
    {
        return {search_size, lookahead_size, out_it, out_it_end...};
    }

    static inline LenDist encode_len_dist(std::size_t len, std::size_t dist)
    {
        assert(len >= min_len); assert(dist >= 1);
        return { static_cast<Len>(len - static_cast<std::size_t>(min_len)),
                 static_cast<Dist>(dist - 1) };
    }

    static inline std::pair<std::size_t, std::size_t> decode_len_dist(LenDist lendist)
    {
        return decode_len_dist(lendist.len, lendist.dist);
    }

    static inline std::pair<std::size_t, std::size_t> decode_len_dist(Len len, Dist dist)
    {
        return std::make_pair(static_cast<std::size_t>(len) + static_cast<std::size_t>(min_len),
                              static_cast<std::size_t>(dist) + 1);
    }

    static std::size_t get_nr_literals_within_token(const Token& token)
    {
        return std::visit(utils::Overloaded {
                [](std::monostate) -> std::size_t { return 0; },
                [](const Literal&) -> std::size_t { return 1; },
                [](const LenDist& lendist) { return decode_len_dist(lendist).first; }
            }, token);
    }
};

/* Sliding window utility for LZ77. */
template<LZ77Policy Policy>
template<std::input_iterator InIt, typename... InItEnd>
class LZ77<Policy>::SlidingWindow {
public:
    SlidingWindow(std::size_t search_size, std::size_t lookahead_size, InIt in_it, InItEnd... in_it_end)
        : search_size(search_size), lookahead_size(lookahead_size), seq(in_it, in_it_end...)
    {
        refetch_data();
    }

    template<std::input_iterator PrevIt, typename... PrevItEnd>
    SlidingWindow(SlidingWindow<PrevIt, PrevItEnd...>&& prev, InIt in_it, InItEnd... in_it_end)
        :
        search_size(prev.search_size), lookahead_size(prev.lookahead_size),
        search(std::move(prev.search)), lookahead(std::move(prev.lookahead)),
        offset(prev.offset),
        seq(in_it, in_it_end...)
    {
        refetch_data();
    }

    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    SlidingWindow(const SlidingWindow<PrevInIt, PrevInItEnd...>& prev, InIt in_it, InItEnd... in_it_end)
        :
        search_size(prev.search_size), lookahead_size(prev.lookahead_size),
        search(prev.search), lookahead(prev.lookahead),
        offset(prev.offset),
        seq(in_it, in_it_end...)
    {
        refetch_data();
    }

    inline std::size_t get_curr_search_size() const noexcept
    {
        return search.size();
    }

    inline std::size_t get_offset() const noexcept
    {
        return offset;
    }

    inline std::size_t get_curr_peek_pos() const noexcept
    {
        return offset + search.size();
    }

    /* Peek at the lookahead window. Returns the iterator at which the peek stopped. */
    template<std::output_iterator<Literal> OutIt>
    OutIt peek(OutIt peek_it, std::size_t n)
    {
        refetch_data();
        return std::copy_n(lookahead.begin(), std::min(n, lookahead.size()), peek_it);
    }

    /* Advance by one literal. Assuming the lookahead window is not empty, and thus
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

    /* Advance by the given length of literals. The given length shall not exceed a match length
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

    /* Find longest match starting from the given position. Assuming pos >= offset. */
    std::pair<std::size_t, std::size_t> find_match(std::size_t pos) const
    {
        assert(pos >= offset);

        auto search_it = search.begin() + (pos - offset);
        auto lookah_it = lookahead.begin();
        const auto search_it_end = search.end();
        const auto lookah_it_end = lookahead.end();

        std::tie(search_it, lookah_it) = mismatch(search_it, search_it_end, lookah_it, lookah_it_end);
        if (search_it == search_it_end) // search overflow case
            lookah_it = std::mismatch(lookah_it, lookah_it_end, lookahead.begin()).first;

        const std::size_t match_len = std::distance(lookahead.begin(), lookah_it);
        const std::size_t match_dist = search.size() - (pos - offset);
        return std::make_pair(match_len, match_dist);
    }

    inline auto& get_it() noexcept
    {
        return seq.it;
    }

    inline const auto& get_it() const noexcept
    {
        return seq.it;
    }

    template<std::input_iterator NextIt, typename... NextInItEnd>
    inline auto continue_by(NextIt it, NextInItEnd... it_end) &&
    {
        return SlidingWindow<NextIt, NextInItEnd...>(std::move(*this), it, it_end...);
    }

    template<std::input_iterator NextIt, typename... NextInItEnd>
    inline auto continue_by(NextIt it, NextInItEnd... it_end) const &
    {
        return SlidingWindow<NextIt, NextInItEnd...>(*this, it, it_end...);
    }

private:
    /* Custom implementation of std::mismatch with addition of checking both iterator ends. */
    template<std::input_iterator ItL, std::input_iterator ItR>
    static std::pair<ItL, ItR> mismatch(ItL it_l, ItL it_l_end, ItR it_r, ItR it_r_end)
    {
        for (; it_l != it_l_end && it_r != it_r_end && *it_l == *it_r; ++it_l, ++it_r);
        return std::make_pair(it_l, it_r);
    }

    inline void refetch_data()
    {
        for (; seq.is_valid() && lookahead.size() < lookahead_size; ++seq.it)
            lookahead.push_back(*seq.it);
    }

    std::size_t search_size, lookahead_size;
    std::deque<Literal> search, lookahead;
    std::size_t offset = 0;
    misc::Sequence<InIt, InItEnd...> seq;
};

template<LZ77Policy Policy>
template<std::input_iterator InIt, typename... InItEnd>
class LZ77<Policy>::Encoder {
public:
    using KeySeq = std::array<Literal, min_len>;

    struct KeySeqHasher {
        std::size_t operator()(const KeySeq& min_seq) const noexcept
        {
            std::bitset<min_len * sizeof(Literal) * CHAR_BIT> bitset;
            for (std::size_t i = 0; i < min_len; ++i) {
                bitset <<= sizeof(Literal);
                bitset |= min_seq[i];
            }
            return std::hash<decltype(bitset)>{}(bitset);
        }
    };

    Encoder(std::size_t search_size, std::size_t lookahead_size, InIt in_it, InItEnd... in_it_end)
        : window(search_size, lookahead_size, in_it, in_it_end...)
    {
        if (search_size < min_len || lookahead_size < min_len)
            throw Exception<Encoder>("search or lookahead sizes less than the min length");
    }

    Token encode_once()
    {
        KeySeq key_seq;
        auto key_seq_it = window.peek(key_seq.begin(), key_seq.size());
        if (key_seq_it == key_seq.begin())
            return std::monostate();
        if (key_seq_it != key_seq.end()) {
            window.advance_once();
            return key_seq.front();
        }

        std::forward_list<std::size_t>& pos_chain = hash_chains[key_seq];
        auto [max_match_len, max_match_dist] = find_best_match(pos_chain);

        if (0 == max_match_len) {
            pos_chain.push_front(window.get_curr_peek_pos());
            window.advance_once();
            return key_seq.front();
        }

        std::size_t _tmp_max_len_to_update_pos_chain = 8;
        if (max_match_len <= _tmp_max_len_to_update_pos_chain)
            pos_chain.push_front(window.get_curr_peek_pos());
        // TODO: implement lazy matching

        window.advance(max_match_len);

        return encode_len_dist(max_match_len, max_match_dist);
    }

    inline InIt& get_it() noexcept
    {
        return window.get_it();
    }

    inline const InIt& get_it() const noexcept
    {
        return window.get_it();
    }

private:
    std::pair<std::size_t, std::size_t> find_best_match(std::forward_list<std::size_t>& pos_chain)
    {
        std::size_t max_match_len = 0, max_match_dist = 0;
        auto pos_it = pos_chain.begin();
        for (; pos_it != pos_chain.end() && *pos_it >= window.get_offset(); ++pos_it) {
            const auto [match_len, match_dist] = window.find_match(*pos_it);
            if (match_len > max_match_len) {
                max_match_len = match_len;
                max_match_dist = match_dist;
            }
        }
        if (pos_it != pos_chain.end())
            pos_chain.erase_after(pos_it, pos_chain.end());
        return std::make_pair(max_match_len, max_match_dist);
    }

    SlidingWindow<InIt, InItEnd...> window;
    std::unordered_map<KeySeq, std::forward_list<std::size_t>, KeySeqHasher> hash_chains;
    Token cached_token;
};

template<LZ77Policy Policy>
template<std::output_iterator<typename Policy::Literal> OutIt, typename... OutItEnd>
class LZ77<Policy>::Decoder {
public:
    Decoder(std::size_t search_size, std::size_t lookahead_size, OutIt out_it, OutItEnd... out_it_end)
        :   lookahead_size(lookahead_size),
            full_size(search_size + lookahead_size),
            seq(out_it, out_it_end...)
    {
    }

    void decode_once(const Literal& literal)
    {
        if (!seq.is_valid())
            return;
        *seq.it = literal; ++seq.it;
        window.push_back(literal);
    }

    inline auto decode_once(LenDist lendist)
    {
        return decode_once(lendist.len, lendist.dist);
    }

    Error<Decoder> decode_once(Len len_code, Dist dist_code)
    {
        auto [len, dist] = decode_len_dist(len_code, dist_code);

        if (dist > window.size()) [[unlikely]]
            return "invalid distance decoded, points further behind data";
        if (len > lookahead_size) [[unlikely]]
            return "too large length decoded";

        for (; len > 0 && seq.is_valid(); --len) {
            const Literal literal = window[window.size() - dist];
            *seq.it = literal;
            window.push_back(literal);
        }

        const std::size_t redundant_size = window.size() - std::min(window.size(), full_size);
        window.erase(window.begin(), window.begin() + redundant_size);

        return success;
    }

    Error<Decoder> decode_once(const Token& token)
    {
        return std::visit(utils::Overloaded {
                [](std::monostate) -> Error<Decoder>
                {
                    return success;
                },
                [this](const LZ77<>::Literal& literal) -> Error<Decoder>
                {
                    decode_once(literal);
                    return success;
                },
                [this](const LZ77<>::LenDist& lendist) -> Error<Decoder>
                {
                    return decode_once(lendist);
                },
            }, token);
    }

    inline OutIt& get_it()
    {
        return seq.it;
    }

    inline const OutIt& get_it() const
    {
        return seq.it;
    }

private:
    std::size_t lookahead_size, full_size;
    std::deque<Literal> window;
    misc::Sequence<OutIt, OutItEnd...> seq;
};

}
