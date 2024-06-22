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
    uint8_t major;
    uint8_t minor;
    uint16_t patch;
};

constexpr SemVer version = { SQUEEZE_VERSION_MAJOR, SQUEEZE_VERSION_MINOR, SQUEEZE_VERSION_PATCH };

}
