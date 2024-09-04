#pragma once

#include <concepts>

namespace squeeze::compression {

template<typename T>
concept LZ77Policy = requires
    {
        typename T::Literal; std::copyable<typename T::Literal>;
        requires requires (typename T::Literal a, typename T::Literal b)
        {
            { a == b } -> std::convertible_to<bool>;
        };
        typename T::Len;  requires std::integral<typename T::Len>;
        typename T::Dist; requires std::integral<typename T::Dist>;
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
