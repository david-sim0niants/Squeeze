#pragma once

#include <algorithm>

#include "compression/compression.h"

namespace squeeze {

using compression::CompressionParams;

template<std::input_iterator In, std::output_iterator<char> Out>
void decode(In in, std::size_t size, Out out, const CompressionParams& compression) noexcept
{
    std::copy_n(in, size, out); // temporary
}

}
