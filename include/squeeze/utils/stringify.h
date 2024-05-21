#pragma once

#include <string>
#include <string_view>
#include <concepts>

namespace squeeze::utils {

template<typename T>
std::string stringify(const T& obj)
{
    return "<unpresentable>";
}

template<std::integral Integral>
inline std::string stringify(Integral integral)
{
    return std::to_string(integral);
}

template<std::floating_point Floating>
inline std::string stringify(Floating floating)
{
    return std::to_string(floating);
}

template<> inline std::string stringify<const char *>(const char * const& str)
{
    return std::string(str);
}

template<> inline std::string stringify<std::string>(const std::string& str)
{
    return str;
}

template<> inline std::string stringify<std::string_view>(const std::string_view& str)
{
    return std::string(str);
}

}
