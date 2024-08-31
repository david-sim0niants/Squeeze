#pragma once

#include <algorithm>
#include <istream>
#include <ostream>

#include "compression/params.h"
#include "error.h"

namespace squeeze {

using compression::CompressionParams;

/* Decode a char stream of a given size from another char stream using the compression info provided. */
Error<> decode(std::ostream& out, std::size_t size, std::istream& in,
               const CompressionParams& compression);

}
