#pragma once

#include <cstddef>
#include <cassert>
#include <climits>
#include <utility>
#include <bitset>
#include <deque>
#include <unordered_map>
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
 * Another parameter defines how long a best-found match should be to skip insertion into the hash chain. */
template<LZ77Policy Policy>
template<std::input_iterator InIt, typename... InItEnd>
class LZ77<Policy>::Encoder {
private:
    /* A sequence of symbols of min_match_len size used for finding matches in the hash chain. */
    using ChainKey = std::bitset<min_match_len * sizeof(Sym) * CHAR_BIT>;
    /* Chain type used in the hash chains. */
    using Chain = std::deque<std::size_t>;
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
            search_window(search_window),
            nr_fetched_syms(nr_fetched_syms),
            hash_chains(prev.hash_chains),
            seq(in_it, in_it_end...),
            cached_token(prev.cached_token)
    {
    }

    /* Construct as a continuation of another encoder with new iterator(s). */
    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    Encoder(Encoder<PrevInIt, PrevInItEnd...>&& prev, InIt in_it, InItEnd... in_it_end)
        :   params(std::move(prev.params)),
            search_window(std::move(search_window)),
            nr_fetched_syms(std::move(nr_fetched_syms)),
            hash_chains(std::move(prev.hash_chains)),
            seq(in_it, in_it_end...),
            cached_token(std::move(prev.cached_token))
    {
    }

    /* Encode and generate a single token. */
    Token encode_once()
    {
        if (cached_token.get_type() != Token::None) [[unlikely]]
            return std::exchange(cached_token, Token()); // return the cached token from previous calculations

        fetch_min_match_data();
        assert(nr_fetched_syms <= min_match_len);

        if (0 == nr_fetched_syms) [[unlikely]]
            return {}; // no fetched symbols, the encoding is done with the current input, return none token

        if (min_match_len > nr_fetched_syms) { // insufficient fetched symbols to do any match
            const Sym sym = *(search_window.get_pivot() - nr_fetched_syms);
            mark_processed(1);
            return sym; // return a symbol token
        }

        // try to find the best match
        const Sym sym = *(search_window.get_pivot() - min_match_len);
        auto [match_len, match_dist] = find_better_match(min_match_len - 1);

        std::size_t nr_processed_fetch_syms = match_len;

        if (match_len <= params.lazy_match_threshold) {
            // longest match not "long enough", try lazy matching
            auto [lazy_match_len, lazy_match_dist] = try_lazy_matching(match_len);
            if (lazy_match_len > match_len) { // lazy matching succeeded
                cached_token = Token(lazy_match_len, lazy_match_dist); // caching it
                match_len = 1; match_dist = 0; // current match will be a single-symbol match
                nr_processed_fetch_syms = lazy_match_len + match_len;
            }
        }

        mark_processed(nr_processed_fetch_syms);
        assert(nr_fetched_syms <= min_match_len);

        if (1 == match_len)
            return sym;
        else
            return {match_len, match_dist};
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
        while (can_fetch_sym() && nr_fetched_syms < min_match_len)
            fetch_sym();
    }

    /* Mark the specified number of fetched symbols as processed. */
    inline void mark_processed(std::size_t nr_processed_fetch_syms)
    {
        assert(nr_fetched_syms >= nr_processed_fetch_syms);
        nr_fetched_syms -= nr_processed_fetch_syms;
    }

    /* Construct a chain key from the min match symbols starting from the specified iterator. */
    inline static void construct_chain_key(CircularIterator it, ChainKey& chain_key)
    {
        for (std::size_t i = 0; i < min_match_len; ++i, ++it) {
            chain_key <<= sizeof(Sym) * CHAR_BIT;
            chain_key |= static_cast<std::make_unsigned_t<Sym>>(*it);
        }
    }

    /* Try lazy matching by skipping one symbol and finding a better match with the next one. */
    std::tuple<std::size_t, std::size_t> try_lazy_matching(std::size_t match_len)
    {
        match_len = std::max(match_len, min_match_len);
        assert(nr_fetched_syms >= match_len);
        if (match_len == nr_fetched_syms && !try_fetch_sym())
            return std::make_tuple(0, 0);
        else
            return find_better_match(match_len - 1);
    }

    /* Find the best match that is strictly longer than the specified length.
     * Otherwise return {1, 0} ("only one symbol matched and it matched to itself"). */
    std::tuple<std::size_t, std::size_t> find_better_match(std::size_t match_len)
    {
        assert(match_len + 1 >= min_match_len);
        std::size_t match_dist = 0;

        const bool peak_match = find_better_match_from_hash_chains(match_len, match_dist);

        // try to find an overlapping match, even if the match reached its peak,
        // as smaller distances are more favorable
        std::size_t overlapping_match_len = match_len - !!(peak_match);
        find_better_match_overlapping_min_match(overlapping_match_len, match_dist);
        // branchlessly get the new match length
        match_len = overlapping_match_len + !!(overlapping_match_len < match_len);

        // if the match distance found so far is 0, then there's no repeated sequence to match,
        // so the match length should be 1, meaning a single-symbol match
        if (0 == match_dist)
            match_len = 1;

        return std::make_tuple(match_len, match_dist);
    }

    /* Find the best match from the hash chains that is strictly longer than the specified match length.
     * Return whether the match reached its peak. */
    bool find_better_match_from_hash_chains(std::size_t& match_len, std::size_t& match_dist)
    {
        assert(match_len + 1 >= min_match_len);

        ChainKey chain_key {};
        construct_chain_key(search_window.get_pivot() - (match_len + 1), chain_key);
        const std::size_t min_match_pos = search_window.get_end_pos() - (match_len + 1 - min_match_len);

        Chain& pos_chain = hash_chains[chain_key];
        const bool peak_match = find_better_match_from_chain(pos_chain, match_len, match_dist);

        if ((0 == match_dist ? 1 : match_len) <= params.match_insert_threshold)
            pos_chain.push_front(min_match_pos);

        return peak_match;
    }

    /* Find the best match from the specified position chain that is strictly
     * longer than the specified match length. Return whether the match reached its peak. */
    bool find_better_match_from_chain(Chain& pos_chain, std::size_t& match_len, std::size_t& match_dist)
    {
        assert(match_len + 1 >= min_match_len);

        auto pos_it = pos_chain.begin();

        // skip too recent matches that are currently beyond the end of the search window
        for (; pos_it != pos_chain.end() && *pos_it >= search_window.get_end_pos(); ++pos_it);

        bool peak_match = false;
        for (; pos_it != pos_chain.end() && *pos_it >= search_window.get_pos() && !peak_match; ++pos_it)
            peak_match = match_and_extend_at<min_match_len>(*pos_it, match_len, match_dist);

        // erase too early matches that are behind the search window
        pos_chain.erase(pos_it, pos_chain.end());

        return peak_match;
    }

    /* Find the best match that overlaps the min match symbols with small distances in the
     * range [1, min_match_len - 1]. Return whether the match reached its peak. */
    bool find_better_match_overlapping_min_match(std::size_t& match_len, std::size_t& match_dist)
    {
        assert(search_window.get_end_pos() >= match_len + 1);
        std::size_t pos = search_window.get_end_pos() - match_len - 1;
        for (std::size_t i = 1; i < min_match_len && pos > search_window.get_filled_pos(); ++i)
            if (match_and_extend_at<0>(--pos, match_len, match_dist))
                return true;
        return false;
    }

    /* Find a match at the specified position with the specified length and if it's found, try to
     * extend it by fetching more symbols. Return whether the match reached its peak.
     * When the match reaches its peak, no fetched symbol is left unmatched, otherwise number
     * of fetched symbols is one bigger than the extended size. */
    template<std::size_t pos_shift>
    bool match_and_extend_at(std::size_t pos, std::size_t& match_len, std::size_t& match_dist)
    {
        assert(match_len + 1 >= pos_shift);

        const std::size_t expected_match_len = match_len + 1 - pos_shift;
        assert(search_window.get_end_pos() >= pos + expected_match_len);
        if (not search_window.suffix_matches_at(pos, expected_match_len))
            return false;

        ++match_len;
        match_dist = search_window.get_end_pos() - pos - match_len + pos_shift;

        auto match_it = search_window.get_iter_at(pos + match_len - pos_shift);
        return fetch_and_match(match_it, match_len);
    }

    /* Fetch symbols and match against them, extending the match.
     * Return true if the match reached its peak, which is either
     * the match length became equal to the lookahead buffer (conceptual) size,
     * or at some point no more symbols could be fetched to match against.
     * Return false if the matching stopped simply because of a symbol mismatch.
     * In this case, it should be noted that the last unmatched symbol has also
     * been fetched and will appear at the end of the search buffer. */
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
    LZ77EncoderParams params;
    SlidingWindow search_window;
    std::size_t nr_fetched_syms = 0;
    std::unordered_map<ChainKey, Chain> hash_chains;
    misc::Sequence<InIt, InItEnd...> seq;
    Token cached_token;
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
            seq(out_it, out_it_end...)
    {
        // processing the partial token (if not none) in a newly continued decoder
        decode_raw(partial_token);
    }

    /* Construct as a continuation of another decoder with new iterator(s). */
    template<std::output_iterator<Sym> PrevOutIt, typename... PrevOutItEnd>
    Decoder(Decoder<PrevOutIt, PrevOutItEnd...>&& prev, OutIt out_it, OutItEnd... out_it_end)
        :   search_window(search_window),
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

    SlidingWindow search_window;
    misc::Sequence<OutIt, OutItEnd...> seq;
    Token partial_token;
};

}
