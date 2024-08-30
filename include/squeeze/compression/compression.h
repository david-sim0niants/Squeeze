#pragma once

#include "params.h"
#include "config.h"
#include "huffman_15.h"

namespace squeeze::compression {

template<CompressionMethod method, bool use_terminator,
        std::input_iterator InIt, std::output_iterator<char> OutIt, typename ...OutItEnd>
    requires (sizeof...(OutItEnd) <= 1)
std::tuple<OutIt, Error<>> compress(misc::BitEncoder<char, CHAR_BIT, OutIt, OutItEnd...>& bit_encoder,
        InIt in_it, InIt in_it_end)
{
    if constexpr (method == CompressionMethod::Huffman)
        return huffman15_encode<use_terminator>(bit_encoder, in_it, in_it_end);
    else
        throw BaseException("invalid compression method or an unimplemented one");
}

template<CompressionMethod method, bool expect_terminator,
        std::output_iterator<char> OutIt, std::input_iterator InIt, typename ...InItEnd>
    requires (sizeof...(InItEnd) <= 1)
std::tuple<OutIt, Error<>> decompress(
        OutIt out_it, OutIt out_it_end, misc::BitDecoder<char, CHAR_BIT, InIt, InItEnd...>& bit_decoder)
{
    if constexpr (method == CompressionMethod::Huffman)
        return huffman15_decode<expect_terminator>(out_it, out_it_end, bit_decoder);
    else
        throw BaseException("invalid compression method or an unimplemented one");
}

template<CompressionMethod method, bool use_terminator,
         std::input_iterator InIt, std::output_iterator<char> OutIt, std::output_iterator<char>... OutItEnd>
    requires (sizeof...(OutItEnd) <= 1)
std::tuple<InIt, OutIt, Error<>> compress(InIt in_it, InIt in_it_end, OutIt out_it, OutItEnd... out_it_end)
{
    if constexpr (method == CompressionMethod::Huffman)
        return huffman15_encode<use_terminator>(in_it, in_it_end, out_it, out_it_end...);
    else
        throw BaseException("invalid compression method or an unimplemented one");
}

template<CompressionMethod method, bool expect_terminator,
         std::output_iterator<char> OutIt, std::input_iterator InIt, std::input_iterator... InItEnd>
    requires (sizeof...(InItEnd) <= 1)
std::tuple<OutIt, InIt, Error<>> decompress(OutIt out_it, OutIt out_it_end, InIt in_it, InItEnd... in_it_end)
{
    if constexpr (method == CompressionMethod::Huffman)
        return huffman15_decode<expect_terminator>(out_it, out_it_end, in_it, in_it_end...);
    else
        throw BaseException("invalid compression method or an unimplemented one");
}

}
