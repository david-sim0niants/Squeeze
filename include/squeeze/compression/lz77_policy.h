#pragma once

#include <concepts>

namespace squeeze::compression {

/* Concept defining what is expected from an LZ77 policy type. */
template<typename T>
concept LZ77Policy = requires
    {
        // Require a copyable, default-constructible Sym (symbol) type with equality operator
        typename T::Sym;
        std::copyable<typename T::Sym>;
        requires std::is_default_constructible_v<typename T::Sym>;
        requires requires (typename T::Sym a, typename T::Sym b)
        {
            { a == b } -> std::convertible_to<bool>;
        };

        // Require a constexpr minimum length of an integral type
        T::min_len;
        std::integral<decltype(T::min_len)>;
        requires std::is_same_v<decltype(T::min_len), const decltype(T::min_len)>;
        requires T::min_len == T::min_len;
        requires T::min_len > 0;
    };

template<typename Sym_ = char>
struct BasicLZ77Policy {
    using Sym = Sym_;
    static constexpr std::size_t min_len = 3;
};

}
