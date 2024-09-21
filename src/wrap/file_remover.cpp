#include "squeeze/wrap/file_remover.h"

namespace squeeze::wrap {

using namespace std::string_literals;

bool FileRemover::will_remove(std::string_view path, Stat *err)
{
    auto it = squeeze.find(path);
    if (it == squeeze.end()) {
        if (err)
            *err = "non-existent path - " + std::string(path);
        return false;
    }
    squeeze.will_remove(it, err);
    return true;
}

bool FileRemover::will_remove_recursively(std::string_view path,
        const std::function<Stat *()>& get_stat_ptr)
{
    bool at_least_one_path_removed = false;
    for (auto it = squeeze.begin(); it != squeeze.end(); ++it) {
        if (utils::path_within_dir(it->second.path, path)) {
            squeeze.will_remove(it, get_stat_ptr());
            at_least_one_path_removed = true;
        }
    }
    if (not at_least_one_path_removed)
        if (auto *stat = get_stat_ptr())
            *stat = std::move("non-existent path - "s.append(path));
    return at_least_one_path_removed;
}

void FileRemover::will_remove_all(const std::function<Stat *()>& get_stat_ptr)
{
    for (auto it = squeeze.begin(); it != squeeze.end(); ++it)
        squeeze.will_remove(it, get_stat_ptr());
}

}
