#pragma once

#include <cstdint>

#include "squeeze/utils/stringify.h"

namespace squeeze::compression {

enum class CompressionMethod : uint8_t {
    None = 0,
    Huffman,
    Deflate,
};

}

namespace squeeze::utils {

template<> inline std::string stringify(const compression::CompressionMethod& compression_method)
{
    switch (compression_method) {
        using enum compression::CompressionMethod;
    case None:
        return "none";
    case Huffman:
        return "huffman";
    case Deflate:
        return "deflate";
    default:
        return "[unknown]";
    }
}

}
