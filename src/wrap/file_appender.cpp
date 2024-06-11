#include "squeeze/wrap/file_appender.h"

namespace squeeze::wrap {

bool FileAppender::will_append(const std::filesystem::path& path,
        CompressionMethod compression_method, int level, Error<Writer> *err)
{
    std::string better_path;
    if (!check_append_precondit(path, err, better_path))
        return false;
    if (err)
        writer.will_append<FileEntryInput>(*err, std::move(better_path), compression_method, level);
    else
        writer.will_append<FileEntryInput>(std::move(better_path), compression_method, level);

    return true;
}

void FileAppender::will_append_recursively(const std::string_view path,
        CompressionMethod compression_method, int level,
        const std::function<Error<Writer> *()>& get_err_ptr)
{
    namespace fs = std::filesystem;
    will_append(path, compression_method, level, get_err_ptr());
    for (const auto& dir_entry : fs::recursive_directory_iterator(path))
        will_append(dir_entry.path(), compression_method, level, get_err_ptr());
}

void FileAppender::write()
{
    appendee_path_set.clear();
    writer.write();
}

bool FileAppender::check_append_precondit(const std::filesystem::path& path,
        Error<Writer> *err, std::string& better_path)
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

