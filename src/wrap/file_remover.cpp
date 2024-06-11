#include "squeeze/wrap/file_remover.h"

namespace squeeze::wrap {

bool FileRemover::will_remove(const std::string_view path, Error<Writer> *err)
{
    auto it = squeeze.find_path(path);
    if (it == squeeze.end()) {
        if (err)
            *err = "non-existent path - " + std::string(path);
        return false;
    }
    squeeze.will_remove(it, err);
    return true;
}

void FileRemover::will_remove_recursively(const std::string_view path)
{
    for (auto it = squeeze.begin(); it != squeeze.end(); ++it)
        if (utils::path_within_dir(it->second.path, path))
            squeeze.will_remove(it);
}


}
