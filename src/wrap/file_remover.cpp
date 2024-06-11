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

bool FileRemover::will_remove_recursively(const std::string_view path,
        const std::function<Error<Writer> *()>& get_err_ptr)
{
    bool at_least_one_path_removed = false;
    for (auto it = squeeze.begin(); it != squeeze.end(); ++it) {
        if (utils::path_within_dir(it->second.path, path)) {
            squeeze.will_remove(it, get_err_ptr());
            at_least_one_path_removed = true;
        }
    }
    if (not at_least_one_path_removed)
        if (auto *err = get_err_ptr())
            *err = "non-existent path - " + std::string(path);
    return at_least_one_path_removed;
}

void FileRemover::will_remove_all(const std::function<Error<Writer> *()>& get_err_ptr)
{
    for (auto it = squeeze.begin(); it != squeeze.end(); ++it)
        squeeze.will_remove(it, get_err_ptr());
}

}
