#pragma once

#include <cstdint>
#include <string>
#include <istream>
#include <ostream>

#include "compression_method.h"
#include "error.h"
#include "utils/enum.h"

namespace squeeze {

enum class EntryPermissions : uint16_t {
    None = 0,
    OwnerRead   = 0400,
    OwnerWrite  = 0200,
    OwnerExec   = 0100,
    OwnerAll    = OwnerRead | OwnerWrite | OwnerExec,
    GroupRead   = 040,
    GroupWrite  = 020,
    GroupExec   = 010,
    GroupAll    = GroupRead | GroupWrite | GroupExec,
    OthersRead  = 04,
    OthersWrite = 02,
    OthersExec  = 01,
    OthersAll   = OthersRead | OthersWrite | OthersExec,
    All         = OwnerAll | GroupAll | OthersAll,
};
SQUEEZE_DEFINE_ENUM_LOGIC_BITWISE_OPERATORS(EntryPermissions);

enum class EntryType : uint8_t {
    None = 0,
    RegularFile,
    Directory,
    Symlink,
};

struct EntryAttributes {
    EntryType type : 7;
    EntryPermissions permissions : 9;
};

std::istream& operator>>(std::istream& input, EntryAttributes& entry_attributes);
std::ostream& operator<<(std::ostream& output, const EntryAttributes& entry_attributes);

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
        sizeof(attributes) + sizeof(path_len) + sizeof(path);
};

}
