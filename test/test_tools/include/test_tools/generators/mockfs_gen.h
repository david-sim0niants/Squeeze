#pragma once

#include "test_tools/mock/fs.h"
#include "prng.h"

namespace squeeze::test_tools::generators {

mock::FileSystem gen_mockfs(const std::string_view content_seed, const PRNG& prng = {});

}
