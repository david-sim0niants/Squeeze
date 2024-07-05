#include "squeeze/decoder.h"
#include "squeeze/utils/io.h"

namespace squeeze {

Error<> decode(std::istream& in, std::size_t size, std::ostream& out, const CompressionParams& compression)
{
    thread_local char inbuf[BUFSIZ];
    while (size) {
        size_t read_size = std::min(size, static_cast<std::size_t>(BUFSIZ));
        in.read(inbuf, read_size);
        if (utils::validate_stream_fail(in))
            return {"input read error", ErrorCode::from_current_errno().report()};

        decode_chunk(inbuf, read_size, std::ostreambuf_iterator(out), compression);
        if (utils::validate_stream_fail(out))
            return {"output write error", ErrorCode::from_current_errno().report()};

        size -= read_size;
    }
    return success;
}

}
