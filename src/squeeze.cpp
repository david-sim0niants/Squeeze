// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "squeeze/squeeze.h"

#include <unordered_map>

#include "squeeze/logging.h"

namespace squeeze {

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::Squeeze::"

bool Squeeze::update()
{
    SQUEEZE_TRACE();

    std::unordered_multimap<std::string_view, FutureAppend *> appendee_path_map;
    appendee_path_map.reserve(future_appends.size());

    for (auto& future_append : future_appends)
        appendee_path_map.emplace(future_append.entry_input.get_path(), &future_append);

    for (auto it = this->begin(); it != this->end() && !appendee_path_map.empty(); ++it) {
        auto& entry_header = it->second;
        auto node = appendee_path_map.extract(entry_header.path);
        if (node.empty())
            continue;
        will_remove(it, node.mapped()->status);
        SQUEEZE_TRACE("Will update {}", it->second.path);
    }

    return this->write();
}

}
