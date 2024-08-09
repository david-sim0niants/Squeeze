#pragma once

#include <concepts>
#include <climits>

namespace squeeze::compression {

template<typename T>
concept AddableComparable = requires(T a, T b) { { a + b } -> std::same_as<T>; } and
                            requires(T a, T b) { { a < b } -> std::convertible_to<bool>; };

template<typename T>
concept HuffmanCode = std::is_default_constructible_v<T>
    and std::is_copy_constructible_v<T> and std::is_copy_assignable_v<T>
    and requires(T t, int x)
        {
            { ++t } -> std::same_as<T>;
            { t << x } -> std::same_as<T>;
        };

template<typename T>
concept HuffmanPolicy = requires
    {
        typename T::Freq; typename T::CodeLen;
        requires AddableComparable<typename T::Freq>;
        requires std::integral<typename T::CodeLen>;
        { T::code_len_limit } -> std::same_as<const typename T::CodeLen&>;
        requires T::code_len_limit <= sizeof(unsigned long long) * CHAR_BIT;
    };

template<unsigned int codelen_limit> requires (codelen_limit <= sizeof(unsigned long long) * CHAR_BIT)
struct BasicHuffmanPolicy {
    using Freq = unsigned int;
    using CodeLen = unsigned int;
    static constexpr CodeLen code_len_limit = codelen_limit;
};

}
