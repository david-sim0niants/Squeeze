#include "squeeze/utils/io.h"

#include <cstdint>

namespace squeeze::utils {

void iosmove(std::iostream& ios, std::streampos dst, std::streampos src, std::streamsize len)
{
    char buffer[BUFSIZ];

    while (len) {
        const std::streamsize step_len = std::min(len, (std::streamsize)BUFSIZ);

        ios.seekg(src);
        ios.read(buffer, step_len);
        ios.seekp(dst);
        ios.write(buffer, step_len);

        dst += step_len;
        src += step_len;
        len -= step_len;
    }
}

void ioscopy(std::istream& src_stream, std::streampos src_pos,
             std::ostream& dst_stream, std::streampos dst_pos,
             std::streamsize cpy_len)
{
    char buffer[BUFSIZ];

    while (cpy_len) {
        const std::streamsize step_len = std::min(cpy_len, (std::streamsize)BUFSIZ);

        src_stream.seekg(src_pos);
        src_stream.read(buffer, step_len);

        dst_stream.seekp(dst_pos);
        dst_stream.write(buffer, step_len);

        src_pos += step_len;
        dst_pos += step_len;
        cpy_len -= step_len;
    }
}

}