#pragma once

#include <cstddef>

#include "squeeze/utils/stringify.h"

namespace squeeze::compression {

/* Additional runtime encoder params. */
struct LZ77EncoderParams {
    std::size_t lazy_match_threshold; /* Controls the lazy matching behaviour. */
    std::size_t match_insert_threshold; /* Controls match insertion into the hash chain behavior. */
};

}

template<> inline std::string squeeze::utils::stringify(const compression::LZ77EncoderParams& params)
{
    return "{ lazy_match_threshold=" + stringify(params.lazy_match_threshold) +
           ", match_insert_threshold=" + stringify(params.match_insert_threshold) + " }";
}
