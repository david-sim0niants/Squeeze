#include "squeeze/squeeze.h"

#include <unordered_set>

#include "squeeze/logging.h"
#include "writer_internal.h"

namespace squeeze {

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::Squeeze::"

void Squeeze::update()
{
    SQUEEZE_TRACE();

    std::unordered_multiset<std::string_view> appendee_path_set;
    appendee_path_set.reserve(future_appends.size());

    for (const auto& future_append : future_appends)
        appendee_path_set.insert(future_append.entry_input.get_path());

    for (auto it = this->begin(); it != this->end() && !appendee_path_set.empty(); ++it) {
        auto& entry_header = it->second;
        if (appendee_path_set.extract(entry_header.path).empty())
            continue;
        will_remove(it);
        SQUEEZE_TRACE("Will update {}", it->second.path);
    }

    this->write();
}

}
