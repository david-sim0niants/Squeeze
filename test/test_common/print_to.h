#pragma once

#include <ostream>

#include "squeeze/compression/params.h"
#include "squeeze/compression/lz77.h"

namespace squeeze::compression {

inline void PrintTo(const CompressionParams& params, std::ostream *os)
{
    *os << utils::stringify(params);
}

inline void PrintTo(const LZ77EncoderParams& params, std::ostream *os)
{
    *os << utils::stringify(params);
}

}
