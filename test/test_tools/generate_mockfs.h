#pragma once

#include "mockfs.h"
#include "random.h"

#include <vector>

namespace squeeze::testing::tools {

MockFileSystem generate_mockfs(const Random<int>& prng, const std::string_view seed);

}
