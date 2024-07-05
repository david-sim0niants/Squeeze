#include "squeeze/wrap/file_appender.h"

namespace squeeze::wrap {

bool FileAppender::will_append(const std::filesystem::path& path,
        const CompressionParams& compression, Error<Appender> *err)
{
    std::string better_path;
    if (!check_append_precondit(path, err, better_path))
        return false;
    if (err)
        appender.will_append<FileEntryInput>(*err, std::move(better_path), compression);
    else
        appender.will_append<FileEntryInput>(std::move(better_path), compression);

    return true;
}

bool FileAppender::will_append_recursively(const std::string_view path,
        const CompressionParams& compression,
        const std::function<Error<Appender> *()>& get_err_ptr)
{
    namespace fs = std::filesystem;

    if (not will_append(path, compression, get_err_ptr()))
        return false;

    ErrorCode ec;
    if (fs::symlink_status(path, ec.get()).type() != fs::file_type::directory)
        return true;

    for (const auto& dir_entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied))
        will_append(dir_entry.path(), compression, get_err_ptr());

    return true;
}

void FileAppender::perform_appends(unsigned concurrency)
{
    appendee_path_set.clear();
    appender.perform_appends(concurrency);
}

bool FileAppender::check_append_precondit(const std::filesystem::path& path,
        Error<Appender> *err, std::string& better_path)
{
    auto better_path_opt = utils::make_concise_portable_path(path);
    if (!better_path_opt) {
        if (err)
            *err = "no such file or directory - " + path.string();
        return false;
    }

    if (appendee_path_set.contains(*better_path_opt))
        return false;
    appendee_path_set.insert(*better_path_opt);

    better_path.swap(*better_path_opt);
    return true;
}

}

