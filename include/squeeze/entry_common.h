// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <cstdint>

#include "printing.h"
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
    constexpr EntryAttributes() = default;

    constexpr EntryAttributes(EntryType type, EntryPermissions permissions)
        : data((uint16_t(type) << 9) | uint16_t(permissions))
    {
    }

    constexpr inline EntryType get_type() const noexcept
    {
        return EntryType(data >> 9);
    }

    constexpr inline EntryPermissions get_permissions() const noexcept
    {
        return EntryPermissions(data & 0x1FF);
    }

    constexpr inline void set_type(EntryType type) noexcept
    {
        data = (uint16_t(type) << 9) | (data & 0x1FF);
    }

    constexpr inline void set_permissions(EntryPermissions permissions) noexcept
    {
        data = (data & 0xFE00) | (uint16_t(permissions) & 0x1FF);
    }

    uint16_t data = 0;
};

template<> void print_to(std::ostream& os, const EntryAttributes& attributes);

}
