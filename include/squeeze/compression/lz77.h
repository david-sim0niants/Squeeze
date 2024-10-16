// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <cstddef>
#include <cassert>
#include <climits>
#include <utility>
#include <array>
#include <algorithm>

#include "lz77_policy.h"
#include "lz77_params.h"
#include "lz77_token.h"
#include "lz77_sliding_window.h"

#include "squeeze/exception.h"
#include "squeeze/status.h"
#include "squeeze/misc/sequence.h"

namespace squeeze::compression {

/* The LZ77 coding interface. Provides methods for encoding/decoding data
 * using the LZ77 algorithm utilizing sliding windows. */
template<LZ77Policy Policy = BasicLZ77Policy<>>
class LZ77 {
public:
    /* Symbol type */
    using Sym = typename Policy::Sym;
    /* Min match length */
    static constexpr auto min_match_len = Policy::min_match_len;
    /* Search buffer size */
    static constexpr std::size_t search_size = Policy::search_size;
    /* Lookahead buffer size */
    static constexpr std::size_t lookahead_size = Policy::lookahead_size;

    /* Token type */
    using Token = LZ77Token<Sym>;

    /* The sliding window */
    using SlidingWindow = LZ77SlidingWindow<Sym, search_size>;

    template<std::input_iterator InIt, typename... InItEnd>
    class Encoder;
    template<std::output_iterator<typename Policy::Sym> OutIt, typename... OutItEnd>
    class Decoder;

    /* Make an encoder from the provided input iterator(s). */
    template<std::input_iterator InIt, typename... InItEnd>
    static inline auto make_encoder(InIt in_it, InItEnd... in_it_end)
    {
        return Encoder<InIt, InItEnd...>(in_it, in_it_end...);
    }

    /* Make an encoder from the provided params and input iterator(s). */
    template<std::input_iterator InIt, typename... InItEnd>
    static inline auto make_encoder(const LZ77EncoderParams& params, InIt in_it, InItEnd... in_it_end)
    {
        return Encoder<InIt, InItEnd...>(params, in_it, in_it_end...);
    }

    /* Make a decoder from the provided search_size and output iterator(s). */
    template<std::output_iterator<Sym> OutIt, typename... OutItEnd>
    static inline auto make_decoder(OutIt out_it, OutItEnd... out_it_end)
    {
        return Decoder<OutIt, OutItEnd...>(out_it, out_it_end...);
    }
};

/* The LZ77::Encoder class. Utilizes SlidingWindow class for searching and encoding repeated sequences.
 * The sliding window is defined by the provided search size.
 * Supports additional runtime parameters passed by LZ77EncoderParams struct that control some of its behavior.
 * These runtime parameters may define and affect the compression efficiency and performance.
 * Supports lazy-matching by deferring the selection of matches in favor of potentially finding better
 * matches starting from the next symbol. A threshold defined in LZ77EncoderParams controls how long
 * the best-found match should be to skip the lazy match. Greater the value, slower but more
 * efficient may be the compression.
 * Another parameter defines how long a best-found match shall be to skip insertion into its hash chain. */
template<LZ77Policy Policy>
template<std::input_iterator InIt, typename... InItEnd>
class LZ77<Policy>::Encoder {
private:
    /* The circular iterator type. */
    using CircularIterator = typename SlidingWindow::CircularIterator;

public:
    /* Construct from the provided input iterator(s). */
    Encoder(InIt in_it, InItEnd... in_it_end)
        : Encoder({ .lazy_match_threshold = lookahead_size - min_match_len,
                    .match_insert_threshold = lookahead_size - min_match_len }, in_it, in_it_end...)
    {
    }

    /* Construct from the provided params and input iterator(s). */
    Encoder(const LZ77EncoderParams& params, InIt in_it, InItEnd... in_it_end)
        : params(params), seq(in_it, in_it_end...)
    {
        if (0 == params.match_insert_threshold)
            throw Exception<Encoder>("match insert threshold is 0");
    }

    /* Construct as a continuation of another encoder with new iterator(s). */
    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    Encoder(const Encoder<PrevInIt, PrevInItEnd...>& prev, InIt in_it, InItEnd... in_it_end)
        :   params(prev.params),
            search_window(prev.search_window),
            nr_fetched_syms(prev.nr_fetched_syms),
            head(prev.head),
            prev(prev.prev),
            cached_token(prev.cached_token),
            seq(in_it, in_it_end...)
    {
    }

    /* Construct as a continuation of another encoder with new iterator(s). */
    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    Encoder(Encoder<PrevInIt, PrevInItEnd...>&& prev, InIt in_it, InItEnd... in_it_end)
        :   params(std::move(prev.params)),
            search_window(std::move(prev.search_window)),
            nr_fetched_syms(std::move(prev.nr_fetched_syms)),
            head(std::move(prev.head)),
            prev(std::move(prev.prev)),
            cached_token(std::move(prev.cached_token)),
            seq(in_it, in_it_end...)
    {
    }

    /* Encode and generate a single token. */
    Token encode_once()
    {
        if (cached_token.get_type() != Token::None) [[unlikely]]
            return std::exchange(cached_token, Token()); // return the cached token from previous calculations

        Token token = find_longest_match(); // get token for the longest match
        if (token.is_none() || token.get_len() >= params.lazy_match_threshold) {
            // if the token is a none token (zero-length) or is either sufficiently long, return it
            mark_processed(token.get_len());
            return token;
        }

        // otherwise, try lazy matching
        mark_processed(1); // mark the first matched symbol as processed
        cached_token = lazy_find_longest_match(token.get_len());
        if (cached_token.is_len_dist()) { // lazy matching produced a longer match
            assert(cached_token.get_len() > token.get_len());
            mark_processed(cached_token.get_len()); // mark the lazy-matched symbols as processed
            assert(nr_fetched_syms <= min_match_len);
            return token.get_sym(); // return the first symbol in the original match
        } else { // lazy matching didn't produce a longer match
            mark_processed(token.get_len() - 1); // marking the rest of matched symbols as processed
            assert(nr_fetched_syms <= min_match_len);
            return token; // return the full token of the original match
        }
    }

    /* Check if the encoder has finished encoding with the current sequence. */
    inline bool is_finished() const noexcept
    {
        return cached_token.is_none() && 0 == nr_fetched_syms;
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
    /* Find the longest match for the current input. */
    Token find_longest_match()
    {
        fetch_min_match_data();
        if (0 == nr_fetched_syms)
            return {};
        else if (nr_fetched_syms < min_match_len)
            return *(search_window.get_pivot() - nr_fetched_syms);

        const Sym match_sym = *(search_window.get_pivot() - min_match_len);
        std::size_t match_len = min_match_len - 1, match_dist = 0;
        find_longest_rep_seq_match_from(match_len, match_dist);

        if (0 == match_dist)
            match_len = 1;
        return {match_sym, match_len, match_dist};
    }

    /* Skip by one symbol and try to find a longer match. */
    Token lazy_find_longest_match(std::size_t match_len)
    {
        if (match_len >= min_match_len) {
            assert(nr_fetched_syms <= match_len);
            assert(match_len - nr_fetched_syms <= 1);
            if (can_fetch_sym())
                fetch_sym();
            else
                return {};

            if (match_len == nr_fetched_syms) {
                if (can_fetch_sym())
                    fetch_sym();
                else
                    return {};
            }
        } else {
            assert(nr_fetched_syms <= min_match_len);
            match_len = min_match_len - 1;
            fetch_min_match_data();
            if (min_match_len > nr_fetched_syms)
                return {};
            assert(min_match_len == nr_fetched_syms);
        }

        std::size_t match_dist = 0;
        find_longest_rep_seq_match_from(match_len, match_dist);

        if (0 == match_dist)
            return {};
        else
            return {match_len, match_dist};
    }

    [[nodiscard]]
    inline bool can_fetch_sym() const
    {
        return seq.is_valid();
    }

    inline Sym fetch_sym()
    {
        const Sym sym = *seq.it; ++seq.it;
        ++nr_fetched_syms;
        search_window.push_sym(sym);
        return sym;
    }

    [[nodiscard]]
    inline bool try_fetch_sym()
    {
        if (seq.is_valid()) {
            search_window.push_sym(*seq.it);
            ++seq.it;
            ++nr_fetched_syms;
            return true;
        }
        return false;
    }

    /* Fetch the minimal amount of data used for finding matches. */
    inline void fetch_min_match_data()
    {
        while (nr_fetched_syms < min_match_len && can_fetch_sym())
            fetch_sym();
    }

    /* Mark the specified number of fetched symbols as processed. */
    inline void mark_processed(std::size_t nr_processed_fetch_syms)
    {
        assert(nr_fetched_syms >= nr_processed_fetch_syms);
        nr_fetched_syms -= nr_processed_fetch_syms;
    }

    /* Find the longest repeated sequence match that is strictly longer than the specified length. */
    void find_longest_rep_seq_match_from(std::size_t& match_len, std::size_t& match_dist)
    {
        assert(match_len + 1 >= min_match_len);
        const bool peak_match = find_longer_match_from_hash_chains(match_len, match_dist);

        // try to find an overlapping match, even if the match reached its peak,
        // as smaller distances are more favorable
        std::size_t overlapping_match_len = match_len - !!(peak_match);
        find_longer_match_overlapping_min_match(overlapping_match_len, match_dist);
        match_len = overlapping_match_len + !!(overlapping_match_len < match_len);
    }

    /* Find the longest match from hash chains that is strictly longer than
     * the specified match length. Return whether the match reached its peak. */
    bool find_longer_match_from_hash_chains(std::size_t& match_len, std::size_t& match_dist)
    {
        assert(match_len + 1 >= min_match_len);
        assert(match_len + 1 >= nr_fetched_syms);

        const std::size_t min_match_pos = search_window.get_end_pos() - (match_len + 1);
        const std::size_t chain_index = get_chain_index_for(search_window.get_pivot() - (match_len + 1));
        const bool peak_match = find_longer_match_from_chain(head[chain_index], match_len, match_dist);

        if ((0 == match_dist ? 1 : match_len) <= params.match_insert_threshold
            && head[chain_index] < min_match_pos)
            update_hash_chain(chain_index, min_match_pos);

        return peak_match;
    }

    /* Get index for calculated hash for min_match_len amount of symbols, starting
     * from the given iterator, that is used to access the associated hash chain. */
    inline static std::size_t get_chain_index_for(CircularIterator it)
    {
        return get_chain_hash_for(it) % search_size;
    }

    /* Calculate hash for min_match_len amount of symbols starting from the given iterator
     * that is used to access the associated hash chain. */
    inline static std::uintmax_t get_chain_hash_for(CircularIterator it)
    {
        static constexpr unsigned int hash_shift = 5;
        std::uintmax_t key = 0;
        for (std::size_t i = 0; i < min_match_len; ++i, ++it) {
            key <<= sizeof(Sym) * hash_shift;
            key ^= static_cast<std::make_unsigned_t<Sym>>(*it);
        }
        return key;
    }

    /* Find the longest match from the specified position chain that is strictly
     * longer than the specified match length. Return whether the match reached its peak. */
    inline bool find_longer_match_from_chain(std::size_t pos, std::size_t& match_len, std::size_t& match_dist)
    {
        // skip too recent positions (usually added by lazy matching)
        for (; pos + match_len + 1 >= search_window.get_end_pos(); pos = get_prev_pos(pos));

        // find the longest match by iterating to previous positions
        for (; pos >= search_window.get_pos(); pos = get_prev_pos(pos))
            if (match_and_extend_at(pos, match_len, match_dist))
                return true;
        return false;
    }

    /* Get the previous position from the current position within the hash chain. */
    inline std::size_t get_prev_pos(std::size_t cur_pos) const
    {
        assert(0 != cur_pos);
        return prev[cur_pos % search_size];
    }

    /* Add a new position to the hash chain. */
    inline void update_hash_chain(const std::size_t chain_index, const std::size_t new_pos)
    {
        assert(new_pos != head[chain_index]);
        prev[new_pos % search_size] = head[chain_index];
        head[chain_index] = new_pos;
    }

    /* Find the longest match that overlaps the min match symbols with small distances in the
     * range [1, min_match_len - 1]. Return whether the match reached its peak. */
    bool find_longer_match_overlapping_min_match(std::size_t& match_len, std::size_t& match_dist)
    {
        assert(search_window.get_end_pos() >= match_len + 1);
        std::size_t pos = search_window.get_end_pos() - match_len - 1;
        for (std::size_t i = 1; i < min_match_len && pos > search_window.get_filled_pos(); ++i)
            if (match_and_extend_at(--pos, match_len, match_dist))
                return true;
        return false;
    }

    /* Find a match at the specified position with the specified length and if it's found, try to
     * extend it by fetching more symbols. Return whether the match reached its peak.
     * When the match reaches its peak, no fetched symbol is left unmatched, otherwise number
     * of fetched symbols is one bigger than the extended size. */
    bool match_and_extend_at(std::size_t pos, std::size_t& match_len, std::size_t& match_dist)
    {
        assert(search_window.get_end_pos() >= pos + match_len + 1);
        if (not search_window.suffix_matches_at(pos, match_len + 1))
            return false;

        ++match_len;
        match_dist = search_window.get_end_pos() - pos - match_len;
        assert(match_dist > 0);

        auto match_it = search_window.get_iter_at(pos + match_len);
        return fetch_and_match(match_it, match_len);
    }

    /* Fetch symbols and match against them, extending the match.
     * Return true if the match reached its peak, which is either
     * the match length became equal to the lookahead buffer (conceptual) size,
     * or at some point no more symbols could be fetched to match against.
     * Return false if the matching stopped because of a symbol mismatch.
     * In this case the last unmatched symbol has also been fetched
     * and will appear at the end of the search buffer. */
    bool fetch_and_match(CircularIterator match_it, std::size_t& match_len)
    {
        while (true) {
            if (not can_fetch_sym() || match_len >= lookahead_size)
                return true;

            const Sym match_sym = *match_it;
            if (match_sym != fetch_sym())
                return false;

            ++match_it;
            ++match_len;
        }
    }

private:
    LZ77EncoderParams params; /* Parameters */
    SlidingWindow search_window; /* The sliding search window */
    std::size_t nr_fetched_syms = 0; /* Number of fetched symbols */
    std::array<std::size_t, search_size> head {}, prev {}; /* Hash chains. */
    Token cached_token; /* The cached token to be returned in the next encoding step. */
    misc::Sequence<InIt, InItEnd...> seq; /* The input sequence. */
};

/* The LZ77::Decoder class. Decodes what the encoder encodes. */
template<LZ77Policy Policy>
template<std::output_iterator<typename Policy::Sym> OutIt, typename... OutItEnd>
class LZ77<Policy>::Decoder {
public:
    /* Construct from the provided search_size and output iterator(s). */
    Decoder(OutIt out_it, OutItEnd... out_it_end)
        :   search_window(),
            seq(out_it, out_it_end...)
    {
    }

    /* Construct as a continuation of another decoder with new iterator(s). */
    template<std::output_iterator<Sym> PrevOutIt, typename... PrevOutItEnd>
    Decoder(const Decoder<PrevOutIt, PrevOutItEnd...>& prev, OutIt out_it, OutItEnd... out_it_end)
        :   search_window(prev.search_window),
            partial_token(prev.partial_token),
            seq(out_it, out_it_end...)
    {
        // processing the partial token (if not none) in a newly continued decoder
        decode_raw(partial_token);
    }

    /* Construct as a continuation of another decoder with new iterator(s). */
    template<std::output_iterator<Sym> PrevOutIt, typename... PrevOutItEnd>
    Decoder(Decoder<PrevOutIt, PrevOutItEnd...>&& prev, OutIt out_it, OutItEnd... out_it_end)
        :   search_window(std::move(prev.search_window)),
            partial_token(std::move(prev.partial_token)),
            seq(out_it, out_it_end...)
    {
        // processing the partial token (if not none) in a newly continued decoder
        decode_raw(partial_token);
    }

    /* Decode a symbol. */
    void decode_once(const Sym& sym)
    {
        decode_sym_raw(sym);
    }

    /* Decode length and distance codes. */
    StatStr decode_once(std::size_t len, std::size_t dist)
    {
        if (dist > search_size) [[unlikely]]
            return "invalid distance that points further behind data";
        decode_len_dist_raw(len, dist);
        return success;
    }

    /* Decode a token. */
    StatStr decode_once(const Token& token)
    {
        switch (token.get_type()) {
        case Token::None:
            return success;
        case Token::Symbol:
            decode_once(token.get_sym());
            return success;
        case Token::LenDist:
            return decode_once(token.get_len(), token.get_dist());
        default:
            throw Exception<Decoder>("invalid token type");
        }
    }

    /* Check if the decoder has finished processing tokens. If there's an unprocessed
     * partial token, the decoder will request more output space to continue decoding. */
    inline bool is_finished() const noexcept
    {
        return not partial_token.is_none();
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
    template<std::output_iterator<Sym> NextOutIt, typename... NextOutItEnd>
    inline auto continue_by(NextOutIt in_it, NextOutItEnd... in_it_end) const &
    {
        return Decoder<NextOutIt, NextOutItEnd...>(*this, in_it, in_it_end...);
    }

    /* Continue by using different iterator(s). */
    template<std::output_iterator<Sym> NextOutIt, typename... NextOutItEnd>
    inline auto continue_by(NextOutIt in_it, NextOutItEnd... in_it_end) &&
    {
        return Decoder<NextOutIt, NextOutItEnd...>(std::move(*this), in_it, in_it_end...);
    }

private:
    inline void decode_raw(const Token& token)
    {
        switch (token.get_type()) {
        case Token::None:
            return;
        case Token::Symbol:
            return decode_sym_raw(token.get_sym());
        case Token::LenDist:
            return decode_len_dist_raw(token.get_len(), token.get_dist());
        default:
            throw Exception<Decoder>("invalid token type");
        }
    }

    inline void decode_sym_raw(const Sym& sym)
    {
        if (!seq.is_valid()) {
            partial_token = sym;
            return;
        }

        *seq.it = sym; ++seq.it;
        search_window.push_sym(sym);
        partial_token = Token();
    }

    inline void decode_len_dist_raw(std::size_t len, std::size_t dist)
    {
        for (auto search_it = search_window.get_pivot() - dist;
             len > 0 && seq.is_valid();
             --len, ++seq.it, ++search_it) {
            const Sym sym = *search_it;
            *seq.it = sym;
            search_window.push_sym(sym);
        }
        partial_token = len == 0 ? Token() : Token(len, dist);
    }

    SlidingWindow search_window; /* The sliding search window. */
    Token partial_token; /* Partially processed token to be processed in the next decoding step. */
    misc::Sequence<OutIt, OutItEnd...> seq; /* Output sequence. */
};

}
