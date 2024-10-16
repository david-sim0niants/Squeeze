// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <cstdint>

#include "squeeze/printing.h"

namespace squeeze::compression {

enum class CompressionMethod : uint8_t {
    None = 0,
    Huffman,
    Deflate,
    Unknown = 0xFF,
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
