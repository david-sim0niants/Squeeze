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

}
