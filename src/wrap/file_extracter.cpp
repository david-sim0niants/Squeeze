#include "squeeze/wrap/file_extracter.h"

namespace squeeze::wrap {

using Stat = FileExtracter::Stat;

Stat FileExtracter::extract(std::string_view path) const
{
    auto it = reader.find(path);
    return it == reader.end() ? "non-existing path - " + std::string(path) : reader.extract(it);
}

bool FileExtracter::extract_recursively(const std::string_view path,
        const std::function<Stat *()>& get_stat_ptr)
{
    bool at_least_one_path_extracted = false;
    for (auto it = reader.begin(); it != reader.end(); ++it) {
        if (!utils::path_within_dir(it->second.path, path))
            continue;
        at_least_one_path_extracted = true;
        Stat *stat = get_stat_ptr();
        if (stat)
            *stat = reader.extract(it);
        else
            reader.extract(it);
    }

    Stat *stat = nullptr;
    if (not at_least_one_path_extracted and (stat = get_stat_ptr()))
        *stat = "non-existent path - " + std::string(path);

    return at_least_one_path_extracted;
}

void FileExtracter::extract_all(const std::function<Stat *()>& get_stat_ptr)
{
    for (auto it = reader.begin(); it != reader.end(); ++it)
        if (auto *stat = get_stat_ptr())
            *stat = reader.extract(it);
}

}
