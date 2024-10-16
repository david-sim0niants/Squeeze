// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <cstddef>
#include <limits>
#include <ostream>

namespace squeeze::test_tools {

void print_data(const char *data, std::size_t size, std::ostream& os,
        std::size_t print_size_limit=std::numeric_limits<std::size_t>::max());

}
