#pragma once

#include "mockfs.h"

#include <vector>

namespace squeeze::testing::tools {

extern const std::string mockfs_seed;

MockFileSystem generate_mockfs(const std::string_view seed = mockfs_seed);

}
