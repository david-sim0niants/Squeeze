#pragma once

#include <concepts>

namespace squeeze::compression {

/* Concept defining what is expected from an LZ77 policy type. */
template<typename T>
concept LZ77Policy = requires
    {
        // Require a copyable, default-constructible Literal with equality operator
        typename T::Literal;
        std::copyable<typename T::Literal>;
        requires std::is_default_constructible_v<typename T::Literal>;
        requires requires (typename T::Literal a, typename T::Literal b)
        {
            { a == b } -> std::convertible_to<bool>;
        };

        // Require integral types of length (Len) and distance (Dist)
        typename T::Len;  requires std::integral<typename T::Len>;
        typename T::Dist; requires std::integral<typename T::Dist>;

        // Require a constexpr minimum length of an integral type
        T::min_len;
        std::integral<decltype(T::min_len)>;
        requires std::is_same_v<decltype(T::min_len), const decltype(T::min_len)>;
        requires T::min_len == T::min_len;
        requires T::min_len > 0;
    };

template<typename Literal_ = char,
    std::integral Len_ = unsigned char,
    std::integral Dist_ = unsigned short>
struct BasicLZ77Policy {
    using Literal = Literal_;
    using Len = Len_;
    using Dist = Dist_;
    static constexpr Len min_len = sizeof(Len) + sizeof(Dist);
};

}
