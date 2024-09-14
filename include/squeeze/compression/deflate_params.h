#pragma once

#include "squeeze/compression/lz77_params.h"
#include "squeeze/utils/stringify.h"

namespace squeeze::compression {

struct DeflateParams {
    LZ77EncoderParams lz77_encoder_params;
};

}

template<> inline std::string squeeze::utils::stringify(const compression::DeflateParams& params)
{
    return "{lz77_encoder_params=" + utils::stringify(params.lz77_encoder_params) + " }";
}
