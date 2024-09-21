#pragma once

#include <cstddef>

#include "squeeze/printing.h"

namespace squeeze::compression {

/* Additional runtime encoder params. */
struct LZ77EncoderParams {
    std::size_t lazy_match_threshold; /* Controls the lazy matching behaviour. */
    std::size_t match_insert_threshold; /* Controls match insertion into the hash chain behavior. */
};

}

template<> inline void squeeze::print_to(std::ostream& os, const compression::LZ77EncoderParams& params)
{
    return print_to(os, "{ lazy_match_threshold=", params.lazy_match_threshold,
                        ", match_insert_threshold=", params.match_insert_threshold, " }");
}
