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
