// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include "method.h"

#include "squeeze/printing.h"

namespace squeeze::compression {

struct CompressionParams {
    CompressionMethod method = CompressionMethod::None;
    uint8_t level = 0;
};

}

template<> inline void squeeze::print_to(std::ostream& os, const compression::CompressionParams& compression)
{
    print_to(os, "{ method=", compression.method, ", level=", compression.level, " }");
}
