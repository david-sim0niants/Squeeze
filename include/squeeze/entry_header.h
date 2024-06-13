#pragma once

#include <string>
#include <istream>
#include <ostream>

#include "entry_common.h"

#include "compression_method.h"
#include "error.h"
#include "utils/enum.h"

namespace squeeze {

struct EntryHeader {
    uint64_t content_size = 0;
    CompressionMethod compression_method = CompressionMethod::None;
    uint8_t compression_level = 0;
    EntryAttributes attributes;
    uint32_t path_len = 0;
    std::string path;

    inline constexpr uint64_t get_header_size() const
    {
        return static_size + path_len;
    }

    inline constexpr uint64_t get_total_size() const
    {
        return static_size + path_len + content_size;
    }

    static Error<EntryHeader> encode(std::ostream& output, const EntryHeader& entry_header);
    static Error<EntryHeader> decode(std::istream& input, EntryHeader& entry_header);

    static constexpr size_t static_size =
        sizeof(content_size) + sizeof(compression_method) + sizeof(compression_level) +
        sizeof(attributes) + sizeof(path_len);
};

template<> std::string utils::stringify(const EntryHeader& header);

}
