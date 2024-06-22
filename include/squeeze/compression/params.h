#pragma once

#include "method.h"

#include "squeeze/utils/stringify.h"

namespace squeeze::compression {

struct CompressionParams {
    CompressionMethod method = CompressionMethod::None;
    uint8_t level = 0;
};

}

namespace squeeze::utils {

template<> inline std::string stringify(const compression::CompressionParams& compression)
{
    return "{ method=" + stringify(compression.method) +
           ", level=" + stringify(compression.level) + " }";
}

}
