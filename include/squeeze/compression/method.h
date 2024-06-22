#pragma once

#include <cstdint>

#include "squeeze/utils/stringify.h"

namespace squeeze::compression {

enum class CompressionMethod : uint8_t {
    None = 0,
};

}

namespace squeeze::utils {

template<> inline std::string stringify(const compression::CompressionMethod& compression_method)
{
    switch (compression_method) {
        using enum compression::CompressionMethod;
    case None:
        return "none";
    default:
        return "[unknown]";
    }
}

}
