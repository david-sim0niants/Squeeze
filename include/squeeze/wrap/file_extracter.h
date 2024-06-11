#pragma once

#include "squeeze/reader.h"
#include "squeeze/utils/fs.h"

#include <functional>

namespace squeeze::wrap {

class FileExtracter {
public:
    explicit FileExtracter(Reader& reader) : reader(reader)
    {}

    bool extract_recursively(const std::string_view path,
            const std::function<Error<Reader> *()>& get_err_ptr = [](){return nullptr;});

    void extract_all(const std::function<Error<Reader> *()>& get_err_ptr = [](){return nullptr;});

    inline auto& get_wrappee()
    {
        return reader;
    }

private:
    Reader& reader;
};

}
