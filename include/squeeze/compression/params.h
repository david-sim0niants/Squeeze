#pragma once

#include "method.h"

#include "squeeze/utils/stringify.h"

namespace squeeze::compression {

struct CompressionParams {
    CompressionMethod method = CompressionMethod::None;
    uint8_t level = 0;
};

}

template<> inline std::string squeeze::utils::stringify(const compression::CompressionParams& compression)
{
    return "{ method=" + stringify(compression.method) +
           ", level=" + stringify(compression.level) + " }";
}
