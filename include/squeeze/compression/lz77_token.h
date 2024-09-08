#pragma once

#include <cstddef>
#include <cassert>
#include <type_traits>
#include <concepts>

namespace squeeze {

/* The encoded token that either holds a symbol (with len = 1, dist = 0),
 * len/dist pair (with len != 0 and dist != 0) or none of them. */
template<typename Sym> requires std::is_default_constructible_v<Sym> and std::copyable<Sym>
class LZ77Token {
public:
    enum Type {
        None = 0, Symbol = 1, LenDist = 2,
    };

    /* Construct a none token. */
    constexpr LZ77Token() = default;

    /* Construct a symbol token. */
    constexpr LZ77Token(Sym sym) : sym(sym), len(1), dist(0)
    {
    }

    /* Construct a len/dist pair token. */
    constexpr LZ77Token(std::size_t len, std::size_t dist) : len(len), dist(dist)
    {
        assert(len != 0 && dist != 0);
    }

    constexpr inline bool is_none() const noexcept
    {
        return len == 0;
    }

    constexpr inline bool is_symbol() const noexcept
    {
        return len == 1 && dist == 0;
    }

    constexpr inline bool is_len_dist() const noexcept
    {
        return len != 0 && dist != 0;
    }

    /* Get token type. Does some tricky math to branchlessly compute type value from len and dist values. */
    constexpr inline Type get_type() const noexcept
    {
        return Type(is_symbol() + 2 * is_len_dist());
    }

    constexpr inline Sym get_sym() const noexcept
    {
        return sym;
    }

    constexpr inline std::size_t get_len() const noexcept
    {
        return len;
    }

    constexpr inline std::size_t get_dist() const noexcept
    {
        return dist;
    }

    /* Get number of symbols that will be decoded from the token. */
    constexpr inline std::size_t get_nr_syms_within() const noexcept
    {
        assert(len != 0 || len == 0 && dist != 0);
        return len;
    }

private:
    Sym sym {};
    std::size_t len = 0;
    std::size_t dist = 0;
};

}
