// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "squeeze/encode.h"

#include "squeeze/logging.h"
#include "squeeze/compression/compression.h"
#include "squeeze/utils/io.h"

namespace squeeze {

EncodeStat encode_buffer(const Buffer& in, Buffer& out, const CompressionParams& compression)
{
    if (compression.method == compression::CompressionMethod::None) {
        std::copy(in.begin(), in.end(), std::back_inserter(out));
        return success;
    }

    using namespace compression;
    using enum compression::CompressionFlags;

    const bool final_block = in.size() < compression::get_block_size(compression);
    const CompressionFlags flags = utils::switch_flag(FinalBlock, final_block);
    return std::get<2>(compress(in.begin(), in.end(), compression, flags, std::back_inserter(out))).status;
}

EncodeStat encode(std::istream& in, std::size_t size, std::ostream& out,
            const compression::CompressionParams& compression)
{
    SQUEEZE_TRACE();

    if (compression.method == compression::CompressionMethod::None) {
        SQUEEZE_TRACE("Compression method is none, plain copying...");
        EncodeStat s = utils::ioscopy(in, in.tellg(), out, out.tellp(), size);
        return s ? success : EncodeStat{"failed copying stream", s};
    }

    const std::size_t buffer_size = compression::get_block_size(compression);

    Buffer inbuf(buffer_size);
    Buffer outbuf;

    while (size) {
        inbuf.resize(std::min(buffer_size, size));
        in.read(inbuf.data(), inbuf.size());
        size -= inbuf.size();
        if (utils::validate_stream_fail(in)) [[unlikely]] {
            SQUEEZE_ERROR("Input read error");
            return "input read error";
        }
        inbuf.resize(in.gcount());

        auto s = encode_buffer(inbuf, outbuf, compression);
        if (s.failed()) [[unlikely]] {
            SQUEEZE_ERROR("Failed encoding buffer");
            return {"failed encoding buffer", s};
        }

        out.write(outbuf.data(), outbuf.size());
        if (utils::validate_stream_fail(out)) [[unlikely]] {
            SQUEEZE_ERROR("output write error");
            return "output write error";
        }
        outbuf.clear();
    }

    return success;
}

}
