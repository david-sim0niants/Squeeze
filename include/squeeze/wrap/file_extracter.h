#pragma once

#include "squeeze/reader.h"
#include "squeeze/utils/fs.h"

#include <functional>

namespace squeeze::wrap {

/* Wrapper over the Reader interface for providing additional extract methods
 * specifically designed for handling files. */
class FileExtracter {
public:
    explicit FileExtracter(Reader& reader) : reader(reader)
    {}

    /* Extract recursively all entries within the path.
     * get_err_ptr() is supposed to provide a pointer to the subsequent error. */
    bool extract_recursively(const std::string_view path,
            const std::function<Error<Reader> *()>& get_err_ptr = [](){return nullptr;});

    /* Extract all entries.
     * get_err_ptr() is supposed to provide a pointer to the subsequent error. */
    void extract_all(const std::function<Error<Reader> *()>& get_err_ptr = [](){return nullptr;});

    inline auto& get_wrappee()
    {
        return reader;
    }

private:
    Reader& reader;
};

}
