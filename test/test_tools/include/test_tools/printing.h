#pragma once

#include <cstddef>
#include <limits>
#include <ostream>

namespace squeeze::test_tools {

void print_data(const char *data, std::size_t size, std::ostream& os,
        std::size_t print_size_limit=std::numeric_limits<std::size_t>::max());

}
