#pragma once

#include "deflate_huffman.h"
#include "lz77.h"

namespace squeeze::compression {

class Deflate {
};

template<bool final_block, std::input_iterator InIt, std::output_iterator<char> OutIt, typename... OutItEnd>
    requires (sizeof...(OutItEnd) <= 1)
std::tuple<InIt, Error<>> deflate(misc::BitEncoder<char, CHAR_BIT, OutIt, OutItEnd...>& bit_encoder,
        InIt in_it, InIt in_it_end)
{
    throw BaseException(__PRETTY_FUNCTION__ + std::string(" is unimplemented"));
}

template<std::output_iterator<char> OutIt, std::input_iterator InIt, typename... InItEnd>
    requires (sizeof...(InItEnd) <= 1)
std::tuple<OutIt, Error<>> inflate(InIt in_it, InIt in_it_end,
        misc::BitDecoder<char, CHAR_BIT, InIt, InItEnd...>& bit_decoder)
{
    throw BaseException(__PRETTY_FUNCTION__ + std::string(" is unimplemented"));
}

template<bool final_block, std::input_iterator InIt, std::output_iterator<char> OutIt, typename... OutItEnd>
    requires (sizeof...(OutItEnd) <= 1)
std::tuple<InIt, OutIt, Error<>> deflate(InIt in_it, InIt in_it_end, OutIt out_it, OutItEnd... out_it_end)
{
    throw BaseException(__PRETTY_FUNCTION__ + std::string(" is unimplemented"));
}

template<std::output_iterator<char> OutIt, std::input_iterator InIt, typename... InItEnd>
    requires (sizeof...(InItEnd) <= 1)
std::tuple<OutIt, InIt, Error<>> inflate(OutIt out_it, OutIt out_it_end, InIt in_it, InItEnd... in_it_end)
{
    throw BaseException(__PRETTY_FUNCTION__ + std::string(" is unimplemented"));
}

}
