#pragma once

#include <algorithm>
#include <istream>
#include <ostream>

#include "compression/compression.h"
#include "error.h"

namespace squeeze {

using compression::CompressionParams;

Error<> decode(std::istream& in, std::size_t size, std::ostream& out, const CompressionParams& compression);

template<std::input_iterator In, std::output_iterator<std::iter_value_t<In>> Out>
std::size_t decode_chunk(In in, std::size_t size, Out out, const CompressionParams& compression)
{
    std::copy_n(in, size, out); // temporary
    return size;
}

}
