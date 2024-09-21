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
