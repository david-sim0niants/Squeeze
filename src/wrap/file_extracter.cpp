#include "squeeze/wrap/file_extracter.h"

namespace squeeze::wrap {

void FileExtracter::extract_recursively(const std::string_view path,
        const std::function<Error<Reader> *()>& get_err_ptr)
{
    for (auto it = reader.begin(); it != reader.end(); ++it) {
        if (!utils::path_within_dir(it->second.path, path))
            continue;
        auto *err = get_err_ptr();
        if (err)
            *err = reader.extract(it);
        else
            reader.extract(it);
    }
}

}
