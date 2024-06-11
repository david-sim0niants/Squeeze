#pragma once

#include "squeeze/writer.h"
#include "squeeze/utils/fs.h"

#include <filesystem>
#include <functional>
#include <unordered_set>

namespace squeeze::wrap {

class FileAppender {
public:
    explicit FileAppender(Writer& writer) : writer(writer)
    {}

    inline bool will_append(std::string&& path,
            CompressionMethod compression_method, int level, Error<Writer> *err = nullptr)
    {
        return will_append(std::filesystem::path(std::move(path)), compression_method, level, err);
    }

    bool will_append(const std::filesystem::path& path,
            CompressionMethod compression_method, int level, Error<Writer> *err = nullptr);

    bool will_append_recursively(const std::string_view path,
            CompressionMethod compression_method, int level,
            const std::function<Error<Writer> *()>& get_err_ptr = [](){ return nullptr; });

    void write();

    inline auto& get_wrappee()
    {
        return writer;
    }

protected:
    inline void clear_appendee_pathset()
    {
        appendee_path_set.clear();
    }

private:
    bool check_append_precondit(const std::filesystem::path& path, Error<Writer> *err,
            std::string& better_path);

    Writer& writer;
    std::unordered_set<std::string> appendee_path_set;
};

}
