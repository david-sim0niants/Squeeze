// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <concepts>
#include <climits>

namespace squeeze::compression {

namespace detail {
    template<typename T>
    concept AddableComparable = requires(T a, T b) { { a + b } -> std::same_as<T>; } and
                                requires(T a, T b) { { a < b } -> std::convertible_to<bool>; };
}

/** Concept defining what is expected from a Huffman policy type. */
template<typename T>
concept HuffmanPolicy = requires
    {
        // Require a frequency type with addable and comparable properties.
        typename T::Freq;    requires detail::AddableComparable<typename T::Freq>;
        // Require an integral code length type.
        typename T::CodeLen; requires std::integral<typename T::CodeLen>;
        // Require a code length limit of the code length type.
        { T::code_len_limit } -> std::same_as<const typename T::CodeLen&>;
        // Require a static limit on the code length limit.
        requires T::code_len_limit <= sizeof(unsigned long long) * CHAR_BIT;
    };

template<unsigned int codelen_limit> requires (codelen_limit <= sizeof(unsigned long long) * CHAR_BIT)
struct BasicHuffmanPolicy {
    using Freq = unsigned int;
    using CodeLen = unsigned int;
    static constexpr CodeLen code_len_limit = codelen_limit;
};

}
