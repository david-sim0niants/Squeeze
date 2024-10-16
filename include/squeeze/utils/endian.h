// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <bit>
#include <cstdint>
#include <concepts>

namespace squeeze::utils {

#if defined(__GNUC__) || defined(__clang__)
constexpr inline uint16_t byteswap16(uint16_t x)
{
    return __builtin_bswap16(x);
}

constexpr inline uint32_t byteswap32(uint32_t x)
{
    return __builtin_bswap32(x);
}

constexpr inline uint64_t byteswap64(uint64_t x)
{
    return __builtin_bswap64(x);
}
#else
constexpr inline uint16_t byteswap16(uint16_t x)
{
    return (x >> 8) | (x << 8);
}

constexpr inline uint32_t byteswap32(uint32_t x)
{
    x = ((x & 0xFF00FF00) >> 8) | ((x & 0x00FF00FF) << 8);
    return (x >> 16) | (x << 16);
}

constexpr inline uint64_t byteswap64(uint64_t x)
{
    x = ((x & 0xFF00FF00FF00FF00ULL) >> 8) | ((x & 0x00FF00FF00FF00FFULL) << 8);
    x = ((x & 0xFFFF0000FFFF0000ULL) >> 16) | ((x & 0x0000FFFF0000FFFFULL) << 16);
    return (x >> 32) | (x << 32);
}
#endif

template<std::integral T>
constexpr inline T byte_swap(T v)
{
    if constexpr (sizeof(T) == 1)
        return v;
    else if constexpr (sizeof(T) == 2)
        return static_cast<T>(byteswap16(static_cast<std::make_unsigned_t<T>>(v)));
    else if constexpr (sizeof(T) == 4)
        return static_cast<T>(byteswap32(static_cast<std::make_unsigned_t<T>>(v)));
    else if constexpr (sizeof(T) == 8)
        return static_cast<T>(byteswap64(static_cast<std::make_unsigned_t<T>>(v)));
    else
        static_assert(sizeof(T) <= 8);
}

template<std::endian endian, std::integral T>
constexpr inline T to_endian_val(T v)
{
    if constexpr (std::endian::native != endian)
        return byte_swap(v);
    else
        return v;
}

template<std::endian endian, std::integral T>
constexpr inline T from_endian_val(T v)
{
    if constexpr (std::endian::native != endian)
        return byte_swap(v);
    else
        return v;
}

}
