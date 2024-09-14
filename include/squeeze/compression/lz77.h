#pragma once

#include <cstddef>
#include <cassert>
#include <climits>
#include <utility>
#include <bitset>
#include <array>
#include <deque>
#include <unordered_map>
#include <algorithm>

#include "lz77_policy.h"
#include "lz77_params.h"
#include "lz77_token.h"
#include "lz77_sliding_window.h"

#include "squeeze/exception.h"
#include "squeeze/error.h"
#include "squeeze/misc/sequence.h"

namespace squeeze::compression {

/* The LZ77 coding interface. Provides methods for encoding/decoding data
 * using the LZ77 algorithm utilizing sliding windows.
 * Its policy defines its symbol, length, distance code types, and some constraints. */
template<LZ77Policy Policy = BasicLZ77Policy<>>
class LZ77 {
public:
    using Sym = typename Policy::Sym;
    static constexpr auto min_len = Policy::min_len;

    using Token = LZ77Token<Sym>;

    template<std::input_iterator InIt, typename... InItEnd>
    using SlidingWindow = LZ77SlidingWindow<Sym, InIt, InItEnd...>;

    template<std::input_iterator InIt, typename... InItEnd>
    class Encoder;
    template<std::output_iterator<typename Policy::Sym> OutIt, typename... OutItEnd>
    class Decoder;

    /* Make an encoder from the provided search_size, lookahead_size, and input iterator(s). */
    template<std::input_iterator InIt, typename... InItEnd>
    static inline auto make_encoder(std::size_t search_size, std::size_t lookahead_size,
            InIt in_it, InItEnd... in_it_end)
    {
        return Encoder<InIt, InItEnd...>(search_size, lookahead_size, in_it, in_it_end...);
    }

    /* Make an encoder from the provided params, search_size, lookahead_size, and input iterator(s). */
    template<std::input_iterator InIt, typename... InItEnd>
    static inline auto make_encoder(const LZ77EncoderParams& params,
            std::size_t search_size, std::size_t lookahead_size, InIt in_it, InItEnd... in_it_end)
    {
        return Encoder<InIt, InItEnd...>(params, search_size, lookahead_size, in_it, in_it_end...);
    }

    /* Make a decoder from the provided search_size, lookahead_size and output iterator(s). */
    template<std::output_iterator<Sym> OutIt, typename... OutItEnd>
    static inline auto make_decoder(std::size_t search_size, std::size_t lookahead_size,
            OutIt out_it, OutItEnd... out_it_end)
    {
        return Decoder<OutIt, OutItEnd...>(search_size, lookahead_size, out_it, out_it_end...);
    }
};

/* The LZ77::Encoder class. Utilizes SlidingWindow class for searching and encoding repeated sequences.
 * The sliding window is defined by the provided search and lookahead sizes. Supports additional runtime
 * parameters passed by LZ77EncoderParams struct that control some of its behavior.
 * These runtime parameters may define and affect the compression efficiency and performance.
 * Supports lazy-matching by deferring the selection of matches in favor of potentially finding better
 * matches starting from the next symbol. A threshold defined in LZ77EncoderParams controls how long
 * the best-found match should be to skip the lazy match. Greater the value, slower but more
 * efficient may be the compression.
 * Another parameter defines how long a best-found match should be to skip insertion into the hash chain. */
template<LZ77Policy Policy>
template<std::input_iterator InIt, typename... InItEnd>
class LZ77<Policy>::Encoder {
public:
    /* A sequence of symbols of min_len size used for finding matches in the hash chain. */
    using KeySeq = std::array<Sym, min_len>;

    /* Hasher for the KeySeq to be passed to the hash chain map. */
    struct KeySeqHasher {
        std::size_t operator()(const KeySeq& min_seq) const noexcept
        {
            std::bitset<min_len * sizeof(Sym) * CHAR_BIT> bitset;
            for (std::size_t i = 0; i < min_len; ++i) {
                bitset <<= sizeof(Sym);
                bitset |= min_seq[i];
            }
            return std::hash<decltype(bitset)>{}(bitset);
        }
    };

    /* Construct from the provided search_size, lookahead_size, and input iterator(s). */
    Encoder(std::size_t search_size, std::size_t lookahead_size,
            InIt in_it, InItEnd... in_it_end)
        : Encoder({ .lazy_match_threshold = lookahead_size - min_len,
                    .match_insert_threshold = lookahead_size - min_len },
                   search_size, lookahead_size, in_it, in_it_end...)
    {
    }

    /* Construct from the provided params, search_size, lookahead_size, and input iterator(s). */
    Encoder(const LZ77EncoderParams& params, std::size_t search_size, std::size_t lookahead_size,
            InIt in_it, InItEnd... in_it_end)
        : params(params), window(search_size, lookahead_size, in_it, in_it_end...)
    {
        if (search_size < min_len || lookahead_size < min_len)
            throw Exception<Encoder>("search or lookahead sizes less than the min length");
        if (0 == params.match_insert_threshold)
            throw Exception<Encoder>("match insert threshold is 0");
    }

    /* Construct as a continuation of another encoder with new iterator(s). */
    template<std::input_iterator PrevInIt, typename... PrevInItEnd>
    Encoder(const Encoder<PrevInIt, PrevInItEnd...>& prev, InIt in_it, InItEnd... in_it_end)
        :   params(prev.params),
            window(prev.window, in_it, in_it_end...),
            hash_chains(prev.hash_chains),
            cached_token(prev.cached_token)
    {
    }

    /* Construct as a continuation of another encoder with new iterator(s). */
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
        if (cached_token.get_type() != Token::None) [[unlikely]]
            return std::exchange(cached_token, Token()); // cache token from previous calculations

        auto [sym, match_len, match_dist] = find_best_match();
        if (0 == match_len) {
            return {}; // no match, not a single symbol
        } else if (1 == match_len && 0 == match_dist) {
            window.advance_once();
            return sym; // single symbol match
        }

        // a repeated sequence match
        if (match_len > params.lazy_match_threshold) {
            window.advance(match_len);
            return {match_len, match_dist}; // no need for a lazy match, already "long enough"
        }

        // doing a lazy match
        window.advance_once();
        auto [next_sym, next_match_len, next_match_dist] = find_best_match();

        if (next_match_len >= match_len) {
            // lazy match actually produced a longer match, keeping in the cache token
            cached_token = {next_match_len, next_match_dist};
            window.advance(next_match_len);
            return sym;
        } else {
            // lazy match didn't produce a longer match, dropping
            window.advance(match_len - 1);
            return {match_len, match_dist};
        }
    }

    /* Check if the encoder has finished encoding with the current sequence. */
    inline bool is_finished() const noexcept
    {
        return window.is_empty();
    }

    /* Get current iterator. */
    inline InIt& get_it()
    {
        return window.get_it();
    }

    /* Get current iterator. */
    inline const InIt& get_it() const
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
    /* Find the best match for the current lookahead symbols. */
    std::tuple<Sym, std::size_t, std::size_t> find_best_match()
    {
        if (window.is_empty()) [[unlikely]]
            return std::make_tuple(Sym(), 0, 0);

        KeySeq key_seq;
        auto key_seq_it = window.peek(key_seq.begin(), key_seq.size());

        const Sym sym = key_seq.front();
        if (key_seq_it != key_seq.end())
            return std::make_tuple(sym, 1, 0);

        std::deque<std::size_t>& pos_chain = hash_chains[key_seq];
        auto [max_match_len, max_match_dist] = find_longest_match_from_chain(pos_chain);

        if (max_match_len <= params.match_insert_threshold)
            pos_chain.push_front(window.get_curr_peek_pos());

        return std::make_tuple(sym, max_match_len, max_match_dist);
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
    LZ77EncoderParams params;
    SlidingWindow<InIt, InItEnd...> window;
    std::unordered_map<KeySeq, std::deque<std::size_t>, KeySeqHasher> hash_chains;
    Token cached_token;
};

/* The LZ77::Decoder class. Decodes what the encoder encodes. */
template<LZ77Policy Policy>
template<std::output_iterator<typename Policy::Sym> OutIt, typename... OutItEnd>
class LZ77<Policy>::Decoder {
public:
    /* Construct from the provided search_size, lookahead_size and output iterator(s). */
    Decoder(std::size_t search_size, std::size_t lookahead_size, OutIt out_it, OutItEnd... out_it_end)
        :   lookahead_size(lookahead_size),
            full_size(search_size + lookahead_size),
            seq(out_it, out_it_end...)
    {
    }

    /* Construct as a continuation of another decoder with new iterator(s). */
    template<std::output_iterator<Sym> PrevOutIt, typename... PrevOutItEnd>
    Decoder(const Decoder<PrevOutIt, PrevOutItEnd...>& prev, OutIt out_it, OutItEnd... out_it_end)
        :   lookahead_size(prev.lookahead_size),
            full_size(prev.full_size),
            window(prev.window),
            seq(out_it, out_it_end...)
    {
        // processing the partial token (if not none) in a newly continued decoder
        decode_raw(partial_token);
    }

    /* Construct as a continuation of another decoder with new iterator(s). */
    template<std::output_iterator<Sym> PrevOutIt, typename... PrevOutItEnd>
    Decoder(Decoder<PrevOutIt, PrevOutItEnd...>&& prev, OutIt out_it, OutItEnd... out_it_end)
        :   lookahead_size(prev.lookahead_size),
            full_size(prev.full_size),
            window(std::move(prev.window)),
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
    Error<Decoder> decode_once(std::size_t len, std::size_t dist)
    {
        if (dist > window.size()) [[unlikely]]
            return "invalid distance that points further behind data";
        if (len > lookahead_size) [[unlikely]]
            return "too large length";
        decode_len_dist_raw(len, dist);
        return success;
    }

    /* Decode a token. */
    Error<Decoder> decode_once(const Token& token)
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
     * partial token, the decoder will request more output space to continue to decoding. */
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
        window.push_back(sym);

        if (window.size() > full_size) [[unlikely]]
            window.pop_front();

        partial_token = Token();
    }

    inline void decode_len_dist_raw(std::size_t len, std::size_t dist)
    {
        for (; len > 0 && seq.is_valid(); --len) {
            const Sym sym = window[window.size() - dist];
            *seq.it = sym;
            window.push_back(sym);
        }

        const std::size_t redundant_size = window.size() - std::min(window.size(), full_size);
        window.erase(window.begin(), window.begin() + redundant_size);

        partial_token = len == 0 ? Token() : Token(len, dist);
    }

    std::size_t lookahead_size, full_size;
    std::deque<Sym> window;
    misc::Sequence<OutIt, OutItEnd...> seq;
    Token partial_token;
};

}
