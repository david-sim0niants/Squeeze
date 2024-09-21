#pragma once

#include <cstdint>

#include "squeeze/printing.h"

namespace squeeze::compression {

enum class CompressionMethod : uint8_t {
    None = 0,
    Huffman,
    Deflate,
};

}

template<> inline void squeeze::print_to(std::ostream& os,
        const compression::CompressionMethod& compression_method)
{
    switch (compression_method) {
        using enum compression::CompressionMethod;
    case None:
        return print_to(os, "none");
    case Huffman:
        return print_to(os, "huffman");
    case Deflate:
        return print_to(os, "deflate");
    default:
        return print_to(os, "[unknown]");
    }
}
