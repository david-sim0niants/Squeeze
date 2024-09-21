#pragma once

#include <ostream>
#include <sstream>

namespace squeeze {

template<typename T> inline void print_to(std::ostream& os, const T& obj)
{
    os << "<unpresentable>";
}

template<typename T> requires (std::integral<T> || std::floating_point<T> ||
        std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> ||
        std::is_same_v<T, const char *> ||
        std::is_array_v<T> && std::is_same_v<char, std::remove_extent_t<T>>)
inline void print_to(std::ostream& os, const T& obj)
{
    os << obj;
}

inline void print_to(std::ostream& os, const bool& b)
{
    os << (b ? "true" : "false");
}

inline void print_to(std::ostream& os, unsigned char c)
{
    os << static_cast<unsigned int>(c);
}

template<typename... Ts> requires (sizeof...(Ts) > 1)
inline void print_to(std::ostream& os, const Ts&... objs)
{
    (print_to(os, objs), ...);
}

template<typename T>
inline std::string stringify(const T& obj)
{
    std::ostringstream oss;
    print_to(oss, obj);
    return std::move(oss).str();
}

template<typename... T>
inline std::string stringify(const T&... objs)
{
    std::ostringstream oss;
    (print_to(oss, objs), ...);
    return std::move(oss).str();
}

}
