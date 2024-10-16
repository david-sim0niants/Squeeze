// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include "squeeze/reader.h"
#include "squeeze/utils/fs.h"

#include <functional>

namespace squeeze::wrap {

/** Wrapper over the Reader interface for providing additional extract methods
 * specifically designed for handling files. */
class FileExtracter {
public:
    using Stat = Extracter::Stat;

    explicit FileExtracter(Reader& reader) : reader(reader)
    {}

    /** Extract an entry with the given path to a file. */
    Stat extract(std::string_view path) const;

    /** Extract recursively all entries within the path.
     * get_stat_ptr() is supposed to provide a pointer to the subsequent status */
    bool extract_recursively(const std::string_view path,
            const std::function<Stat *()>& get_stat_ptr = [](){return nullptr;});

    /** Extract all entries.
     * get_stat_ptr() is supposed to provide a pointer to the subsequent status. */
    void extract_all(const std::function<Stat *()>& get_stat_ptr = [](){return nullptr;});

    inline auto& get_wrappee()
    {
        return reader;
    }

private:
    Reader& reader;
};

}
