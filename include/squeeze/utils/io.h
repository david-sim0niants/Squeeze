#pragma once

#include <iostream>

#include "squeeze/common.h"
#include "squeeze/error.h"

namespace squeeze::utils {

void iosmove(std::iostream& ios, std::streampos dst, std::streampos src, std::streamsize len);
Error<> ioscopy(std::istream& src_stream, std::streampos src_pos,
             std::ostream& dst_stream, std::streampos dst_pos,
             std::streamsize cpy_len);

inline bool validate_stream_bad(std::ios& stream)
{
    const bool bad = stream.bad();
    stream.clear();
    return bad;
}

inline bool validate_stream_fail(std::ios& stream)
{
    const bool fail = stream.fail();
    stream.clear();
    return fail;
}

}
