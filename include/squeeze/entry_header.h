#pragma once

#include <string>
#include <istream>
#include <ostream>

#include "entry_common.h"

#include "compression_method.h"
#include "error.h"
#include "utils/enum.h"

namespace squeeze {

/* Struct that contains the entry header data.
 * It can be used both for representing the entry as well as encoding/decoding it. */
struct EntryHeader {
    uint64_t content_size = 0; /* Entry content size */
    CompressionMethod compression_method = CompressionMethod::None; /* Compression method used */
    uint8_t compression_level = 0; /* Compression level used */
    EntryAttributes attributes; /* Entry attributes including its file type and permissions */
    uint32_t path_len = 0; /* Length of the path */
    std::string path; /* The path itself */

    /* Get the header size, including the path length */
    inline constexpr uint64_t get_header_size() const
    {
        return static_size + path_len;
    }

    /* Get full size of the entry, including the content size. */
    inline constexpr uint64_t get_full_size() const
    {
        return static_size + path_len + content_size;
    }

    static Error<EntryHeader> encode(std::ostream& output, const EntryHeader& entry_header);
    static Error<EntryHeader> decode(std::istream& input, EntryHeader& entry_header);

    /* Size of the static part of the encoded header */
    static constexpr size_t static_size =
        sizeof(content_size) + sizeof(compression_method) + sizeof(compression_level) +
        sizeof(attributes) + sizeof(path_len);
};

template<> std::string utils::stringify(const EntryHeader& header);

}
