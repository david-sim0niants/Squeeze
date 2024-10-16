// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <ostream>
#include <vector>

#include "prng.h"

namespace squeeze::test_tools::generators {

std::vector<char> gen_data(const PRNG& prng, const std::string_view data_seed);
std::vector<char> gen_data(const PRNG& prng, const std::string_view data_seed,
        unsigned char noise_probability);

std::vector<char> gen_data(const PRNG& prng, const std::string_view data_seed, std::size_t size);
std::vector<char> gen_data(const PRNG& prng, const std::string_view data_seed,
        std::size_t size, unsigned char noise_probability);

std::string gen_alphanumeric_string(std::size_t size, const PRNG& prng = {});

}
