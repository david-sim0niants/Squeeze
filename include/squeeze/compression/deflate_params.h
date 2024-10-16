// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include "squeeze/compression/lz77_params.h"
#include "squeeze/status.h"
#include "squeeze/utils/enum.h"

namespace squeeze::compression {

/* Deflate header bits per the DEFLATE spec (RFC1951) */
enum class DeflateHeaderBits : std::uint8_t {
    Store = 0b00, // not supported
    FixedHuffman = 0b01, // not supported
    DynamicHuffman = 0b10, // supported
    Reserved = 0b11, // should not be used
    TypeMask = 0b11, // type bitmask
    FinalBlock = 0b100, // indicates a final block of data
};
SQUEEZE_DEFINE_ENUM_LOGIC_BITWISE_OPERATORS(DeflateHeaderBits);

struct DeflateParams {
    DeflateHeaderBits header_bits = DeflateHeaderBits::DynamicHuffman;
    LZ77EncoderParams lz77_encoder_params {};
};

}

template<> inline void squeeze::print_to(std::ostream& os, const compression::DeflateHeaderBits& header_bits)
{
    using namespace std::string_literals;
    using enum compression::DeflateHeaderBits;

    const char *final_str = (((header_bits & FinalBlock) == FinalBlock) ? "FINAL | " : "");
    const char *type_str = "";

    switch (header_bits & TypeMask) {
    case Store:
        type_str = "STORE";
        break;
    case FixedHuffman:
        type_str = "FIXED_HUFFMAN";
        break;
    case DynamicHuffman:
        type_str = "DYNAMIC_HUFFMAN";
        break;
    case Reserved:
        type_str = "RESERVED";
        break;
    default:
        break;
    }
    print_to(os, '[', final_str, type_str, ']');
}

template<> inline void squeeze::print_to(std::ostream& os, const compression::DeflateParams& params)
{
    return print_to(os, "{ header_bits=", params.header_bits,
                        ", lz77_encoder_params=", params.lz77_encoder_params, " }");
}
