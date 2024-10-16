// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "squeeze/utils/io.h"

#include <cstdint>

namespace squeeze::utils {

StatStr iosmove(std::iostream& ios, std::streampos dst, std::streampos src, std::streamsize len)
{
    thread_local char buffer[BUFSIZ] {};
    while (len) {
        const std::streamsize step_len = std::min(len, (std::streamsize)BUFSIZ);

        ios.seekg(src);
        ios.read(buffer, step_len);
        if (utils::validate_stream_fail_eof(ios)) [[unlikely]]
            return "stream read error";
        src += step_len;

        ios.seekp(dst);
        ios.write(buffer, step_len);
        if (utils::validate_stream_fail_eof(ios)) [[unlikely]]
            return "stream write error";
        dst += step_len;

        len -= step_len;
    }
    return success;
}

StatStr ioscopy(std::istream& src_stream, std::streampos src_pos,
             std::ostream& dst_stream, std::streampos dst_pos,
             std::streamsize cpy_len)
{
    thread_local char buffer[BUFSIZ] {};
    src_stream.seekg(src_pos);
    dst_stream.seekp(dst_pos);

    while (cpy_len) {
        const std::streamsize step_len = std::min(cpy_len, (std::streamsize)BUFSIZ);

        src_stream.read(buffer, step_len);
        if (utils::validate_stream_fail_eof(src_stream)) [[unlikely]]
            return "input read error";

        dst_stream.write(buffer, step_len);
        if (utils::validate_stream_fail_eof(dst_stream)) [[unlikely]]
            return "output write error";

        cpy_len -= step_len;
    }

    return success;
}

}
