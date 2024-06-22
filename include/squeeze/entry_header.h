#pragma once

#include <string>
#include <istream>
#include <ostream>

#include "entry_common.h"

#include "compression/params.h"
#include "error.h"
#include "utils/enum.h"

namespace squeeze {

using compression::CompressionParams;

/* Struct that contains the entry header data. */
struct EntryHeader {
    uint64_t content_size = 0; /* Entry content size */
    struct {
        uint8_t major = 0, minor = 0;

    } major_minor_version = {}; /* Major and minor versions */
    CompressionParams compression; /* Compression used */
    EntryAttributes attributes; /* Entry attributes including its file type and permissions */
    std::string path; /* The path itself */

    /* Get the header size, including the path length */
    inline uint64_t get_encoded_header_size() const
    {
        return encoded_static_size + path.size();
    }

    /* Get full size of the entry, including the content size. */
    inline uint64_t get_encoded_full_size() const
    {
        return get_encoded_header_size() + content_size;
    }

    /* Encode the content size.
     * This is useful as the entry header is usually encoded and written before
     * the content but the content size might be unknown before being encoded,
     * thus it might need a re-encoding after the content is encoded and written. */
    static Error<EntryHeader> encode_content_size(std::ostream& output, uint64_t content_size);
    /* Decode the content size. Just mirrors encode_content_size method. */
    static Error<EntryHeader> decode_content_size(std::istream& output, uint64_t& content_size);

    /* Encode the entry header. */
    static Error<EntryHeader> encode(std::ostream& output, const EntryHeader& entry_header);
    /* Decode the entry header. */
    static Error<EntryHeader> decode(std::istream& input, EntryHeader& entry_header);

    using EncodedPathSizeType = uint32_t;

    /* Size of the static part of the encoded header */
    static constexpr size_t encoded_static_size =
        sizeof(major_minor_version) + sizeof(content_size) + sizeof(compression) +
        sizeof(attributes) + sizeof(EncodedPathSizeType);
};

template<> std::string utils::stringify(const EntryHeader& header);

}
