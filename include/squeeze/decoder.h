#pragma once

#include <algorithm>
#include <istream>
#include <ostream>

#include "compression/params.h"
#include "error.h"

namespace squeeze {

using compression::CompressionParams;

Error<> decode(std::istream& in, std::size_t size, std::ostream& out,
               const CompressionParams& compression);

}
