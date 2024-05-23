#pragma once

#include <cstdint>
#include <istream>
#include <ostream>

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

}
