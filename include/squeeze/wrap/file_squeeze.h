// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include "squeeze/squeeze.h"
#include "file_appender.h"
#include "file_remover.h"
#include "file_extracter.h"

namespace squeeze::wrap {

/* This wrapper combines functionality of FileAppender, FileRemover, FileExtracter wrappers
 * and provides one more method - update(). */
class FileSqueeze : public FileAppender, public FileRemover, public FileExtracter {
public:
    explicit FileSqueeze(Squeeze& squeeze)
        : squeeze(squeeze), FileAppender(squeeze), FileRemover(squeeze), FileExtracter(squeeze)
    {}

    /* Calls update() on the squeeze while also resetting its own state. */
    bool update();

    inline auto& get_wrappee()
    {
        return squeeze;
    }

private:
    Squeeze& squeeze;
};

}
