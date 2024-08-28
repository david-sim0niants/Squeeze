#include "squeeze/decoder.h"

#include "squeeze/common.h"
#include "squeeze/compression/compression.h"
#include "squeeze/logging.h"
#include "squeeze/utils/io.h"
#include "squeeze/misc/substream.h"

namespace squeeze {

template<std::input_iterator In>
using BitDecoder = misc::BitDecoder<char, sizeof(char) * CHAR_BIT, In, In>;

template<std::output_iterator<char> Out, std::input_iterator In>
auto decode_block(Out out, Out out_end, In in, In in_end, const CompressionParams& compression)
{
    switch (compression.method) {
        using enum compression::CompressionMethod;
    case Huffman:
        return compression::decompress<Huffman>(out, out_end, in, in_end);
    default:
        throw BaseException("invalid compression method or an unimplemented one");
    }
}

Error<> decode(std::istream& in, std::size_t size, std::ostream& out, const CompressionParams& compression)
{
    SQUEEZE_TRACE();

    if (compression.method == compression::CompressionMethod::None) {
        SQUEEZE_TRACE("Compression method is none, plain copying...");
        auto e = utils::ioscopy(in, in.tellg(), out, out.tellp(), size);
        return e ? Error<>{"failed copying stream", e.report()} : success;
    }

    const std::size_t outbuf_size = compression::get_block_size(compression);
    Buffer outbuf(outbuf_size);
    misc::InputSubstream substream(in, size);

    auto out_it = outbuf.begin();
    auto out_it_end = outbuf.end();
    auto in_it = substream.begin();
    auto in_it_end = substream.end();

    while (in_it != in_it_end) {
        Error<> e;
        std::tie(out_it, in_it, e) = decode_block(out_it, out_it_end, in_it, in_it_end, compression);
        if (e) [[unlikely]] {
            SQUEEZE_ERROR("Failed decoding buffer");
            return {"failed decoding buffer", e.report()};
        }

        if (in_it == in_it_end) {
            if (utils::validate_stream_bad(in)) [[unlikely]] {
                SQUEEZE_ERROR("Input read error");
                return {"input read error", ErrorCode::from_current_errno().report()};
            }
        }

        out.write(outbuf.data(), std::distance(outbuf.begin(), out_it));
        if (utils::validate_stream_fail(out)) [[unlikely]] {
            SQUEEZE_ERROR("Output write error");
            return {"output write error", ErrorCode::from_current_errno().report()};
        }
        out_it = outbuf.begin();
    }

    return success;
}

}
