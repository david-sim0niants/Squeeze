#pragma once

#include <concepts>

namespace squeeze::compression {

/* Concept defining what is expected from an LZ77 policy type. */
template<typename T>
concept LZ77Policy = requires
    {
        // Require an integral Sym (symbol) type
        typename T::Sym;
        std::integral<typename T::Sym>;

        // Require a constexpr min match length of an integral type
        T::min_match_len;
        std::integral<decltype(T::min_match_len)>;
        requires std::is_same_v<decltype(T::min_match_len), const decltype(T::min_match_len)>;
        requires T::min_match_len == T::min_match_len;
        requires T::min_match_len > 0;

        // Require a constexpr search buffer size
        T::search_size;
        requires std::is_same_v<decltype(T::search_size), const std::size_t>;
        requires T::search_size == T::search_size;
        requires T::search_size > 0;

        // Require a constexpr lookahead buffer size
        T::lookahead_size;
        requires std::is_same_v<decltype(T::lookahead_size), const std::size_t>;
        requires T::lookahead_size == T::lookahead_size;
        requires T::lookahead_size > 0;

        requires T::search_size >= T::min_match_len && T::lookahead_size >= T::min_match_len;
    };

template<typename Sym_ = char>
struct BasicLZ77Policy {
    using Sym = Sym_;
    static constexpr std::size_t min_match_len = 3;
    static constexpr std::size_t search_size = 32768;
    static constexpr std::size_t lookahead_size = 258;
};

}
