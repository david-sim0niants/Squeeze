// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include "squeeze/squeeze.h"
#include "squeeze/utils/fs.h"

#include <functional>

namespace squeeze::wrap {

/* Wrapper over the Squeeze interface for providing additional remove methods
 * specifically designed for handling files. */
class FileRemover {
public:
    using Stat = Remover::Stat;

    explicit FileRemover(Squeeze& squeeze) : squeeze(squeeze)
    {
    }

    /* Find an entry by its path and remove it. */
    bool will_remove(std::string_view path, Stat *stat = nullptr);

    /* Remove recursively all entries within the path.
     * get_stat_ptr() is supposed to provide a pointer to the subsequent status. */
    bool will_remove_recursively(std::string_view path,
            const std::function<Stat *()>& get_stat_ptr = [](){ return nullptr; });

    /* Remove all entries.
     * get_stat_ptr() is supposed to provide a pointer to the subsequent status. */
    void will_remove_all(const std::function<Stat *()>& get_stat_ptr = [](){ return nullptr; });

    inline auto& get_wrappee()
    {
        return squeeze;
    }

private:
    Squeeze& squeeze;
};

}
