#pragma once

#include <cstddef>
#include <cassert>
#include <climits>
#include <utility>
#include <bitset>
#include <variant>
#include <array>
#include <deque>
#include <unordered_map>
#include <algorithm>

#include "lz77_policy.h"
#include "squeeze/exception.h"
#include "squeeze/error.h"
#include "squeeze/misc/sequence.h"
#include "squeeze/utils/overloaded.h"

namespace squeeze::compression {

/* The LZ77 coding interface. Provides methods for encoding/decoding data
 * using the LZ77 algorithm utilizing sliding windows.
 * Its policy defines its literal, length, distance code types, and some constraints. */
template<LZ77Policy Policy = BasicLZ77Policy<>>
class LZ77 {
public:
    using Literal = typename Policy::Literal;
    using Len = typename Policy::Len;
    using Dist = typename Policy::Dist;
    static constexpr Len min_len = Policy::min_len;

    /* The length/distance pair. */
    struct LenDist {
        Len len;
        Dist dist;
    };
    /* The encoded token that's either a Literal, LenDist pair or none of them (monostate). */
    using Token = std::variant<std::monostate, Literal, LenDist>;

    template<std::input_iterator InIt, typename... InItEnd>
    class SlidingWindow;

    template<std::input_iterator InIt, typename... InItEnd>
    class Encoder;
    template<std::output_iterator<typename Policy::Literal> OutIt, typename... OutItEnd>
    class Decoder;

    /* Additional runtime encoder params. */
    struct EncoderParams {
        std::size_t lazy_match_threshold; /* Controls the lazy matching behaviour. */
        std::size_t match_insert_threshold; /* Controls match insertion into the hash chain behavior. */
    };

    /* Make an encoder from the provided search_size, lookahead_size, and input iterator(s). */
    template<std::input_iterator InIt, typename... InItEnd>
    static inline Encoder<InIt, InItEnd...> make_encoder(
            std::size_t search_size, std::size_t lookahead_size, InIt in_it, InItEnd... in_it_end)
    {
        return {search_size, lookahead_size, in_it, in_it_end...};
    }

    /* Make an encoder from the provided params, search_size, lookahead_size, and input iterator(s). */
    template<std::input_iterator InIt, typename... InItEnd>
    static inline Encoder<InIt, InItEnd...> make_encoder(const EncoderParams& params,
            std::size_t search_size, std::size_t lookahead_size, InIt in_it, InItEnd... in_it_end)
    {
        return {params, search_size, lookahead_size, in_it, in_it_end...};
    }

    /* Make a decoder from the provided search_size, lookahead_size and output iterator(s). */
    template<std::output_iterator<Literal> OutIt, typename... OutItEnd>
    static inline Decoder<OutIt, OutItEnd...> make_decoder(
            std::size_t search_size, std::size_t lookahead_size, OutIt out_it, OutItEnd... out_it_end)
    {
        return {search_size, lookahead_size, out_it, out_it_end...};
    }

    /* Encode a len/dist pair. Converts them from (std::size_t, std::size_t) to (LenDist) format. */
    static inline LenDist encode_len_dist(std::size_t len, std::size_t dist)
    {
        assert(len >= min_len); assert(dist >= 1);
        return { static_cast<Len>(len - static_cast<std::size_t>(min_len)),
                 static_cast<Dist>(dist - 1) };
    }

    /* Decode a len/dist pair. Converts them from (LenDist) to (std::size_t, std::size_t) format. */
    static inline std::pair<std::size_t, std::size_t> decode_len_dist(LenDist lendist)
    {
        return decode_len_dist(lendist.len, lendist.dist);
    }

    /* Decode a len/dist pair. Converts them from (LenDist) to (std::size_t, std::size_t) format. */
    static inline std::pair<std::size_t, std::size_t> decode_len_dist(Len len, Dist dist)
    {
        return std::make_pair(static_cast<std::size_t>(len) + static_cast<std::size_t>(min_len),
                              static_cast<std::size_t>(dist) + 1);
    }

    /* Get number of literals that will be decoded from the token. */
    static std::size_t get_nr_literals_within_token(const Token& token)
    {
        return std::visit(utils::Overloaded {
                [](std::monostate) -> std::size_t { return 0; },
                [](const Literal&) -> std::size_t { return 1; },
                [](const LenDist& lendist) { return decode_len_dist(lendist).first; }
            }, token);
    }
};

/* Sliding window utility for LZ77. Supports peeking/advancing and finding repeated sequence matches.
 * Handles absolute positions by keeping the offset of its search window.
 * Consists of two consecutive sliding windows - search and lookahead.
 * Advancing pushes literals from the lookahead window front to the search window end. */
template<LZ77Policy Policy>
template<std::input_iterator InIt, typename... InItEnd>
class LZ77<Policy>::SlidingWindow {
public:
    /* Create from the provided search_size, lookahead_size and input iterator(s); */
    SlidingWindow(std::size_t search_size, std::size_t lookahead_size, InIt in_it, InItEnd... in_it_end)
        : search_size(search_size), lookahead_size(lookahead_size), seq(in_it, in_it_end...)
    {
        refetch_data();
    }

    /* Create as a continuation of another sliding window with new iterator(s). */
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

    /* Create as a continuation of another sliding window with new iterator(s). */
    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    SlidingWindow(SlidingWindow<PrevInIt, PrevInItEnd...>&& prev, InIt in_it, InItEnd... in_it_end)
        :
        search_size(prev.search_size), lookahead_size(prev.lookahead_size),
        search(std::move(prev.search)), lookahead(std::move(prev.lookahead)),
        offset(prev.offset),
        seq(in_it, in_it_end...)
    {
        refetch_data();
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

    /* Get the absolute position of the first literal in the lookahead window. */
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
    inline auto& get_it() noexcept
    {
        return seq.it;
    }

    /* Get current iterator. */
    inline const auto& get_it() const noexcept
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
    std::deque<Literal> search, lookahead;
    std::size_t offset = 0;
    misc::Sequence<InIt, InItEnd...> seq;
};

/* The LZ77::Encoder class. Utilizes SlidingWindow class for searching and encoding repeated sequences.
 * The sliding window is defined by the provided search and lookahead sizes. Supports additional runtime
 * parameters passed by EncoderParams struct that control some of its behavior.
 * These runtime parameters may define and affect the compression efficiency and performance.
 * Supports lazy-matching by deferring the selection of matches in favor of potentially finding better
 * matches starting from the next literal. A threshold defined in EncoderParams controls how long
 * the best-found match should be to skip the lazy match. Greater the value, slower but more
 * efficient may be the compression.
 * Another parameter defines how long a best-found match should be to skip insertion into the hash chain. */
template<LZ77Policy Policy>
template<std::input_iterator InIt, typename... InItEnd>
class LZ77<Policy>::Encoder {
public:
    /* A sequence of literals of min_len size used for finding matches in the hash chain. */
    using KeySeq = std::array<Literal, min_len>;

    /* Hasher for the KeySeq to be passed to the hash chain map. */
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

    /* Create from the provided search_size, lookahead_size, and input iterator(s). */
    Encoder(std::size_t search_size, std::size_t lookahead_size,
            InIt in_it, InItEnd... in_it_end)
        : Encoder({ .lazy_match_threshold = lookahead_size - min_len,
                    .match_insert_threshold = lookahead_size - min_len },
                   search_size, lookahead_size, in_it, in_it_end...)
    {
    }

    /* Create from the provided params, search_size, lookahead_size, and input iterator(s). */
    Encoder(const EncoderParams& params, std::size_t search_size, std::size_t lookahead_size,
            InIt in_it, InItEnd... in_it_end)
        : params(params), window(search_size, lookahead_size, in_it, in_it_end...)
    {
        if (search_size < min_len || lookahead_size < min_len)
            throw Exception<Encoder>("search or lookahead sizes less than the min length");
        if (0 == params.match_insert_threshold)
            throw Exception<Encoder>("match insert threshold is 0");
    }

    /* Create as a continuation of another encoder with new iterator(s). */
    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    Encoder(const Encoder<PrevInIt, PrevInItEnd...>& prev, InIt in_it, InItEnd... in_it_end)
        :   params(prev.params),
            window(prev.window, in_it, in_it_end...),
            hash_chains(prev.hash_chains),
            cached_token(prev.cached_token)
    {
    }

    /* Create as a continuation of another encoder with new iterator(s). */
    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    Encoder(Encoder<PrevInIt, PrevInItEnd...>&& prev, InIt in_it, InItEnd... in_it_end)
        :   params(std::move(prev.params)),
            window(std::move(prev.window), in_it, in_it_end...),
            hash_chains(std::move(prev.hash_chains)),
            cached_token(std::move(prev.cached_token))
    {
    }

    /* Encode and generate a single token. */
    Token encode_once()
    {
        if (!std::holds_alternative<std::monostate>(cached_token)) [[unlikely]]
            return std::exchange(cached_token, std::monostate()); // cache token from previous calculations

        auto [literal, match_len, match_dist] = find_best_match();
        if (0 == match_len) {
            return std::monostate(); // no match, not a single character
        } else if (1 == match_len && 0 == match_dist) {
            window.advance_once();
            return literal; // single literal match
        }

        // a repeated sequence match
        if (match_len > params.lazy_match_threshold) {
            window.advance(match_len);
            return encode_len_dist(match_len, match_dist); // no need for a lazy match, already "long enough"
        }

        // doing a lazy match
        window.advance_once();
        auto [next_literal, next_match_len, next_match_dist] = find_best_match();

        if (next_match_len >= match_len) {
            // lazy match actually produced a longer match, keeping in the cache token
            cached_token = encode_len_dist(next_match_len, next_match_dist);
            window.advance(next_match_len);
            return literal;
        } else {
            // lazy match didn't produce a longer match, dropping
            window.advance(match_len - 1);
            return encode_len_dist(match_len, match_dist);
        }
    }

    /* Get current iterator. */
    inline InIt& get_it() noexcept
    {
        return window.get_it();
    }

    /* Get current iterator. */
    inline const InIt& get_it() const noexcept
    {
        return window.get_it();
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
    /* Find the best match for the current lookahead literals. */
    std::tuple<Literal, std::size_t, std::size_t> find_best_match()
    {
        KeySeq key_seq;
        auto key_seq_it = window.peek(key_seq.begin(), key_seq.size());

        if (key_seq_it == key_seq.begin())
            return std::make_tuple(Literal(), 0, 0);

        const Literal literal = key_seq.front();
        if (key_seq_it != key_seq.end())
            return std::make_tuple(literal, 1, 0);

        std::deque<std::size_t>& pos_chain = hash_chains[key_seq];
        auto [max_match_len, max_match_dist] = find_longest_match_from_chain(pos_chain);

        if (max_match_len <= params.match_insert_threshold)
            pos_chain.push_front(window.get_curr_peek_pos());

        return std::make_tuple(literal, max_match_len, max_match_dist);
    }

    /* Find the longest repeated sequence match from the given position chain. */
    std::pair<std::size_t, std::size_t> find_longest_match_from_chain(std::deque<std::size_t>& pos_chain)
    {
        std::size_t max_match_len = 1, max_match_dist = 0;
        auto pos_it = pos_chain.begin();
        for (; pos_it != pos_chain.end() && *pos_it >= window.get_offset(); ++pos_it) {
            const auto [match_len, match_dist] = window.find_match(*pos_it);
            if (match_len > max_match_len) {
                max_match_len = match_len;
                max_match_dist = match_dist;
            }
        }
        // erase old positions that are behind the search window (pos < offset)
        pos_chain.erase(pos_it, pos_chain.end());
        return std::make_pair(max_match_len, max_match_dist);
    }

private:
    EncoderParams params;
    SlidingWindow<InIt, InItEnd...> window;
    std::unordered_map<KeySeq, std::deque<std::size_t>, KeySeqHasher> hash_chains;
    Token cached_token;
};

/* The LZ77::Decoder class. Decodes what the encoder encodes. */
template<LZ77Policy Policy>
template<std::output_iterator<typename Policy::Literal> OutIt, typename... OutItEnd>
class LZ77<Policy>::Decoder {
public:
    /* Create from the provided search_size, lookahead_size and output iterator(s). */
    Decoder(std::size_t search_size, std::size_t lookahead_size, OutIt out_it, OutItEnd... out_it_end)
        :   lookahead_size(lookahead_size),
            full_size(search_size + lookahead_size),
            seq(out_it, out_it_end...)
    {
    }

    /* Create as a continuation of another decoder with new iterator(s). */
    template<std::output_iterator<Literal> PrevOutIt, typename... PrevOutItEnd>
    Decoder(const Decoder<PrevOutIt, PrevOutItEnd...>& prev, OutIt out_it, OutItEnd... out_it_end)
        :   lookahead_size(prev.lookahead_size),
            full_size(prev.full_size),
            window(prev.window),
            seq(out_it, out_it_end...)
    {
    }

    /* Create as a continuation of another decoder with new iterator(s). */
    template<std::output_iterator<Literal> PrevOutIt, typename... PrevOutItEnd>
    Decoder(Decoder<PrevOutIt, PrevOutItEnd...>&& prev, OutIt out_it, OutItEnd... out_it_end)
        :   lookahead_size(prev.lookahead_size),
            full_size(prev.full_size),
            window(std::move(prev.window)),
            seq(out_it, out_it_end...)
    {
    }

    /* Decode a literal. */
    void decode_once(const Literal& literal)
    {
        if (!seq.is_valid())
            return;
        *seq.it = literal; ++seq.it;
        window.push_back(literal);
    }

    /* Decode a pair of length/distance codes. */
    inline auto decode_once(LenDist lendist)
    {
        return decode_once(lendist.len, lendist.dist);
    }

    /* Decode length and distance codes. */
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

    /* Decode a token. */
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

    /* Get current iterator. */
    inline OutIt& get_it()
    {
        return seq.it;
    }

    /* Get current iterator. */
    inline const OutIt& get_it() const
    {
        return seq.it;
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
    std::size_t lookahead_size, full_size;
    std::deque<Literal> window;
    misc::Sequence<OutIt, OutItEnd...> seq;
};

}
