#include "squeeze/utils/io.h"

#include <cstdint>

namespace squeeze::utils {

void iosmove(std::iostream& ios, std::streampos dst, std::streampos src, std::streamsize len)
{
    thread_local char buffer[BUFSIZ] {};
    ios.seekg(src); ios.seekp(dst);
    while (len) {
        const std::streamsize step_len = std::min(len, (std::streamsize)BUFSIZ);
        ios.read(buffer, step_len);
        ios.write(buffer, step_len);
        len -= step_len;
    }
}

Error<> ioscopy(std::istream& src_stream, std::streampos src_pos,
             std::ostream& dst_stream, std::streampos dst_pos,
             std::streamsize cpy_len)
{
    thread_local char buffer[BUFSIZ] {};
    src_stream.seekg(src_pos);
    dst_stream.seekp(dst_pos);

    while (cpy_len) {
        const std::streamsize step_len = std::min(cpy_len, (std::streamsize)BUFSIZ);

        src_stream.read(buffer, step_len);
        if (utils::validate_stream_fail(src_stream)) [[unlikely]]
            return {"input read error", ErrorCode::from_current_errno().report()};

        dst_stream.write(buffer, step_len);
        if (utils::validate_stream_fail(dst_stream)) [[unlikely]]
            return {"output write error", ErrorCode::from_current_errno().report()};

        cpy_len -= step_len;
    }

    return success;
}

}
