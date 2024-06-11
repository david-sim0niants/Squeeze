#pragma once

#include "squeeze/reader.h"
#include "squeeze/utils/fs.h"

#include <iterator>

namespace squeeze::wrap {

class FileExtracter {
public:
    explicit FileExtracter(Reader& reader) : reader(reader)
    {}

    template<std::output_iterator<Error<Reader>> ErrIt>
    void extract_recursively(const std::string_view path, ErrIt err_it, ErrIt err_end)
    {
        for (auto it = reader.begin(); it != reader.end(); ++it) {
            if (!utils::path_within_dir(it->second.path, path))
                continue;
            if (err_it == err_end) [[unlikely]]
                break;
            reader.extract(it, &*err_it);
        }
    }

    void extract_recursively(const std::string_view path);

    inline auto& get_wrappee()
    {
        return reader;
    }

private:
    Reader& reader;
};

}
