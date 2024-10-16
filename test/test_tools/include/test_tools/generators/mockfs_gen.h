// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include "test_tools/mock/fs.h"
#include "prng.h"

namespace squeeze::test_tools::generators {

mock::FileSystem gen_mockfs(const std::string_view content_seed, const PRNG& prng = {});

}
