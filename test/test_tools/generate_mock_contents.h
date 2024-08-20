#pragma once

#include <cstddef>
#include <sstream>

#include "random.h"

namespace squeeze::testing::tools {

struct MockContentsGeneratorParams {
    std::size_t size_limit;
    int min_nr_reps;
    int max_nr_reps;
    unsigned char noise_probability = 25;
    enum {
        NoneFlags = 0,
        ApplyNoise = 1,
        RandomizedRepCount = 2,
        RandomizedRange = 4,
    }; int flags;
};

void generate_mock_contents(const MockContentsGeneratorParams& params,
        const Random<int>& prng, const std::string_view seed, std::ostream& contents);

}
