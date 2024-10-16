// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <algorithm>
#include <istream>
#include <ostream>

#include "compression/params.h"
#include "status.h"

namespace squeeze {

using compression::CompressionParams;
using DecodeStat = StatStr;

/** Decode a char stream of a given size from another char stream using the compression info provided. */
DecodeStat decode(std::ostream& out, std::size_t size, std::istream& in,
        const CompressionParams& compression);

}
