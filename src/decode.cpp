#include "squeeze/decode.h"

#include "squeeze/common.h"
#include "squeeze/compression/compression.h"
#include "squeeze/logging.h"
#include "squeeze/utils/io.h"
#include "squeeze/misc/substream.h"

namespace squeeze {

StatStr decode(std::ostream& out, std::size_t size, std::istream& in,
        const CompressionParams& compression)
{
    using namespace compression;
    using CompressionFlags::ExpectFinalBlock;
    SQUEEZE_TRACE();

    if (compression.method == CompressionMethod::None) {
        SQUEEZE_TRACE("Compression method is none, plain copying...");
        StatStr s = utils::ioscopy(in, in.tellg(), out, out.tellp(), size);
        return s ? success : StatStr{"failed copying stream", s};
    }

    const std::size_t outbuf_size = get_block_size(compression);
    Buffer outbuf(outbuf_size);
    misc::InputSubstream insub(in, size);

    auto out_it = outbuf.begin();
    auto out_it_end = outbuf.end();
    auto in_it = insub.begin();
    auto in_it_end = insub.end();

    while (in_it != in_it_end) {
        DecompressionResult result;
        std::tie(out_it, in_it, result) =
            decompress(out_it, out_it_end, compression, ExpectFinalBlock, in_it, in_it_end);
        if (result.status.failed()) [[unlikely]] {
            SQUEEZE_ERROR("Failed decoding buffer");
            return {"failed decoding buffer", result.status};
        }

        if (in_it == in_it_end) {
            if (utils::validate_stream_fail(in)) [[unlikely]] {
                SQUEEZE_ERROR("Input read error");
                return "input read error";
            }
        }

        out.write(outbuf.data(), std::distance(outbuf.begin(), out_it));
        if (utils::validate_stream_fail_eof(out)) [[unlikely]] {
            SQUEEZE_ERROR("Output write error");
            return "output write error";
        }
        out_it = outbuf.begin();
    }

    return success;
}

}
