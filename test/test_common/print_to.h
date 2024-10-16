// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <ostream>

#include "squeeze/compression/params.h"
#include "squeeze/compression/lz77_params.h"
#include "squeeze/compression/deflate_params.h"

namespace squeeze::compression {

inline void PrintTo(const CompressionParams& params, std::ostream *os)
{
    print_to(*os, params);
}

inline void PrintTo(const LZ77EncoderParams& params, std::ostream *os)
{
    print_to(*os, params);
}

inline void PrintTo(const DeflateParams& params, std::ostream *os)
{
    print_to(*os, params);
}

}
