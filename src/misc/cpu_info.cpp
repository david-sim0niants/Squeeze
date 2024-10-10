#include "squeeze/misc/cpu_info.h"

#if defined (_WIN32) || defined (_WIN64)
#include <windows.h>

namespace squeeze::misc {

std::size_t get_nr_available_cpu_cores()
{
    int process_affinity_mask;
    int system_affinity_mask;

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

