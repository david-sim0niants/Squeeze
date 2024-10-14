#include "squeeze/wrap/file_appender.h"

namespace squeeze::wrap {

namespace fs = std::filesystem;

bool FileAppender::will_append(const std::filesystem::path& path,
        const CompressionParams& compression, Stat *stat)
{
    std::string better_path;
    if (!check_append_precondit(path, stat, better_path))
        return false;
    if (stat)
        appender.will_append<FileEntryInput>(*stat, std::move(better_path), compression);
    else
        appender.will_append<FileEntryInput>(std::move(better_path), compression);
    return true;
}

bool FileAppender::will_append_recursively(const std::string_view path,
        const CompressionParams& compression,
        const std::function<Stat *()>& get_stat_ptr)
{
    if (not will_append(path, compression, get_stat_ptr()))
        return false;

    Status<std::error_code> sc = success;
    if (fs::symlink_status(path, sc.get()).type() != fs::file_type::directory)
        return true;

    using fs::directory_options::skip_permission_denied;
    for (const auto& dir_entry : fs::recursive_directory_iterator(path, skip_permission_denied))
        will_append(dir_entry.path(), compression, get_stat_ptr());

    return true;
}

void FileAppender::perform_appends()
{
    appendee_path_set.clear();
    appender.perform_appends();
}

bool FileAppender::check_append_precondit(const fs::path& path, Stat *stat, std::string& better_path)
{
    auto better_path_opt = utils::make_concise_portable_path(path);
    if (!better_path_opt) {
        if (stat)
            *stat = "no such file or directory - " + path.string();
        return false;
    }

    if (appendee_path_set.contains(*better_path_opt))
        return false;
    appendee_path_set.insert(*better_path_opt);

    better_path.swap(*better_path_opt);
    return true;
}

}

