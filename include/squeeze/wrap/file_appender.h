// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include "squeeze/appender.h"
#include "squeeze/utils/fs.h"

#include <filesystem>
#include <functional>
#include <unordered_set>

namespace squeeze::wrap {

/* Wrapper over the Appender interface for providing additional append methods
 * specifically designed for handling files. */
class FileAppender {
public:
    using Stat = Appender::Stat;

    explicit FileAppender(Appender& appender) : appender(appender)
    {}

    /* Append a file with given path and ensure the same path is not appended more than once. */
    inline bool will_append(std::string&& path, const CompressionParams& compression,
            Stat *stat = nullptr)
    {
        return will_append(std::filesystem::path(std::move(path)), compression, stat);
    }

    /* Append a file with given path and ensure the same path is not appended more than once. */
    bool will_append(const std::filesystem::path& path,
            const CompressionParams& compression, Stat *stat = nullptr);

    /* Append recursively all files within the path.
     * get_stat_ptr() is supposed to provide a pointer to the subsequent status. */
    bool will_append_recursively(const std::string_view path,
            const CompressionParams& compression,
            const std::function<Stat *()>& get_stat_ptr = [](){ return nullptr; });

    /* Calls perform_appends() on the appender while also resetting its own state. */
    void perform_appends();

    inline auto& get_wrappee()
    {
        return appender;
    }

protected:
    inline void clear_appendee_pathset()
    {
        appendee_path_set.clear();
    }

private:
    bool check_append_precondit(const std::filesystem::path& path, Stat *stat,
            std::string& better_path);

    Appender& appender;
    std::unordered_set<std::string> appendee_path_set;
};

}
