#pragma once

#include <iostream>

#include "squeeze/common.h"
#include "squeeze/error.h"

namespace squeeze::utils {

Error<> iosmove(std::iostream& ios, std::streampos dst, std::streampos src, std::streamsize len);
Error<> ioscopy(std::istream& src_stream, std::streampos src_pos,
             std::ostream& dst_stream, std::streampos dst_pos,
             std::streamsize cpy_len);

template<std::ios::iostate include_bits, std::ios::iostate exclude_bits = std::ios::goodbit>
    bool validate_stream_bits(std::ios& stream)
{
    const std::ios::iostate state = stream.rdstate();
    stream.clear(state & ~include_bits);
    return (include_bits == 0 || (state & include_bits) != 0) && state == (state & ~exclude_bits);
}

inline bool validate_stream_bad(std::ios& stream)
{
    return validate_stream_bits<std::ios::badbit>(stream);
}

inline bool validate_stream_fail(std::ios& stream)
{
    return validate_stream_bits<std::ios::failbit | std::ios::badbit , std::ios::eofbit>(stream);
}

inline bool validate_stream_fail_eof(std::ios& stream)
{
    return validate_stream_bits<std::ios::failbit | std::ios::badbit | std::ios::eofbit>(stream);
}

}
