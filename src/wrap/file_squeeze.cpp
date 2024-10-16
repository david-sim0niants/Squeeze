// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "squeeze/wrap/file_squeeze.h"

namespace squeeze::wrap {

bool FileSqueeze::update()
{
    clear_appendee_pathset();
    return squeeze.update();
}

}
