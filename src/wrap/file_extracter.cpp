#include "squeeze/wrap/file_extracter.h"

namespace squeeze::wrap {

bool FileExtracter::extract_recursively(const std::string_view path,
        const std::function<Error<Reader> *()>& get_err_ptr)
{
    bool at_least_one_path_extracted = false;
    for (auto it = reader.begin(); it != reader.end(); ++it) {
        if (!utils::path_within_dir(it->second.path, path))
            continue;
        at_least_one_path_extracted = true;
        auto *err = get_err_ptr();
        if (err)
            *err = reader.extract(it);
        else
            reader.extract(it);
    }

    if (not at_least_one_path_extracted)
        if (auto *err = get_err_ptr())
            *err = "non-existent path - " + std::string(path);
    return at_least_one_path_extracted;
}

}
