// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <cstdint>

#ifndef SQUEEZE_VERSION_MAJOR
#warning "SQUEEZE_VERSION_MAJOR is undefined, defaulting to 0"
#define SQUEEZE_VERSION_MAJOR 0
#endif

#ifndef SQUEEZE_VERSION_MINOR
#warning "SQUEEZE_VERSION_MINOR is undefined, defaulting to 0"
#define SQUEEZE_VERSION_MINOR 0
#endif

#ifndef SQUEEZE_VERSION_PATCH
#warning "SQUEEZE_VERSION_PATCH is undefined, defaulting to 0"
#define SQUEEZE_VERSION_PATCH 0
#endif

namespace squeeze {

struct SemVer {
    constexpr SemVer() noexcept = default;

    constexpr SemVer(uint16_t major, uint16_t minor, uint16_t patch) noexcept
        : data(((major & 0xFFF) << 20) | ((minor & 0x3FF) << 10) | (patch & 0x3FF))
    {
    }

    inline constexpr uint16_t get_major() const noexcept
    {
        return (data >> 20) & 0xFFF;
    }

    inline constexpr uint16_t get_minor() const noexcept
    {
        return (data >> 10) & 0x3FF;
    }

    inline constexpr uint16_t get_patch() const noexcept
    {
        return data & 0x3FF;
    }

    uint32_t data = 0;
};

constexpr SemVer version = { SQUEEZE_VERSION_MAJOR, SQUEEZE_VERSION_MINOR, SQUEEZE_VERSION_PATCH };

}
