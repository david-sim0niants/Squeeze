// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "squeeze/misc/cpu_info.h"

#if defined (_WIN32) || defined (_WIN64)
#include <windows.h>

namespace squeeze::misc {

std::size_t get_nr_available_cpu_cores()
{
    DWORD64 process_affinity_mask;
    DWORD64 system_affinity_mask;

    if (!GetProcessAffinityMask(GetCurrentProcess(), &process_affinity_mask, &system_affinity_mask))
        return 1;

    int count = 0;
    while (process_affinity_mask) {
        if (process_affinity_mask & 1)
            ++count;
        process_affinity_mask >>= 1;
    }
    return count;
}

}
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include <sched.h>
#include <unistd.h>

namespace squeeze::misc {

std::size_t get_nr_available_cpu_cores()
{
    cpu_set_t set;
    CPU_ZERO(&set);

    if (sched_getaffinity(0, sizeof(set), &set) == -1) {
        return -1; // Error getting CPU affinity
    }

    return CPU_COUNT(&set);
}

}
#else
#error "unsupported platform"
#endif

