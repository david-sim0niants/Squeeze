#pragma once

#include <type_traits>

#define SQUEEZE_DEFINE_ENUM_LOGIC_BITWISE_OPERATORS(enum_type)      \
namespace enum_type##_logic_bitwise_operators {                     \
                                                                    \
using ul_type = std::underlying_type_t<enum_type>;                  \
                                                                    \
inline constexpr enum_type operator ~(enum_type e)                  \
{                                                                   \
    return static_cast<enum_type>(~static_cast<ul_type>(e));        \
}                                                                   \
                                                                    \
inline constexpr enum_type operator |(enum_type a, enum_type b)     \
{                                                                   \
    return static_cast<enum_type>(                                  \
        static_cast<ul_type>(a) | static_cast<ul_type>(b));         \
}                                                                   \
                                                                    \
inline constexpr enum_type operator &(enum_type a, enum_type b)     \
{                                                                   \
    return static_cast<enum_type>(                                  \
        static_cast<ul_type>(a) & static_cast<ul_type>(b));         \
}                                                                   \
                                                                    \
inline constexpr enum_type operator ^(enum_type a, enum_type b)     \
{                                                                   \
    return static_cast<enum_type>(                                  \
        static_cast<ul_type>(a) ^ static_cast<ul_type>(b));         \
}                                                                   \
                                                                    \
inline constexpr enum_type &operator |=(enum_type &a, enum_type b)  \
{                                                                   \
    return a = a | b;                                               \
}                                                                   \
                                                                    \
inline constexpr enum_type &operator &=(enum_type &a, enum_type b)  \
{                                                                   \
    return a = a & b;                                               \
}                                                                   \
                                                                    \
inline constexpr enum_type &operator ^=(enum_type &a, enum_type b)  \
{                                                                   \
    return a = a ^ b;                                               \
}                                                                   \
                                                                    \
}                                                                   \
using enum_type##_logic_bitwise_operators::operator~;               \
using enum_type##_logic_bitwise_operators::operator&;               \
using enum_type##_logic_bitwise_operators::operator|;               \
using enum_type##_logic_bitwise_operators::operator^;               \
using enum_type##_logic_bitwise_operators::operator&=;              \
using enum_type##_logic_bitwise_operators::operator|=;              \
using enum_type##_logic_bitwise_operators::operator^=;              \

namespace squeeze::utils {

template<typename T>
concept Enum = std::is_enum_v<T>;

template<Enum E> bool test_flag(E main_flags, E test_flags)
{
    using UType = std::underlying_type_t<E>;
    return (static_cast<UType>(main_flags) & static_cast<UType>(test_flags))
        == static_cast<UType>(test_flags);
}

template<Enum E> constexpr E switch_flag(E flag, bool on)
{
    return static_cast<E>(static_cast<std::underlying_type_t<E>>(flag) * on);
}

}
