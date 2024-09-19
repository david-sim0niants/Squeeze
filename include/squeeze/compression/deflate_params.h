#pragma once

#include "squeeze/compression/lz77_params.h"
#include "squeeze/utils/stringify.h"
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

template<> inline std::string squeeze::utils::stringify(const compression::DeflateHeaderBits& header_bits)
{
    using namespace std::string_literals;
    using enum compression::DeflateHeaderBits;
    std::string str = '[' + (((header_bits & FinalBlock) == FinalBlock) ? "FINAL | "s : ""s);
    switch (header_bits & TypeMask) {
    case Store:
        str += "STORE";
        break;
    case FixedHuffman:
        str += "FIXED_HUFFMAN";
        break;
    case DynamicHuffman:
        str += "DYNAMIC_HUFFMAN";
        break;
    case Reserved:
        str += "RESERVED";
        break;
    default:
        break;
    }
    str += ']';
    return str;
}

template<> inline std::string squeeze::utils::stringify(const compression::DeflateParams& params)
{
    return "{ header_bits=" + stringify(params.header_bits) +
        ", lz77_encoder_params=" + utils::stringify(params.lz77_encoder_params) + " }";
}
