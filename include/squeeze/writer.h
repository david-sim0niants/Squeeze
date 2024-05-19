#pragma once

#include <vector>
#include <variant>
#include <string_view>
#include <fstream>
#include <filesystem>

#include "compression_method.h"

namespace squeeze {

class InsertRequest;

class Writer {
public:
    explicit Writer(const char *path);
    explicit Writer(const std::filesystem::path& path);
    explicit Writer(std::iostream& target);

    void will_insert(std::string&& path, CompressionMethod method, int level);
    void will_insert(std::string&& path, std::istream& input, CompressionMethod method, int level);
    void will_remove(std::string&& path);

    void perform();

    void insert(std::string&& path, CompressionMethod method, int level);
    void insert(std::string&& path, std::istream& input, CompressionMethod method, int level);
    void remove(std::string&& path);

    inline void will_insert(const std::string_view path,
            CompressionMethod method, int level)
    {
        will_insert(std::string(path), method, level);
    }

    inline void will_insert(const std::string_view path, std::istream& input,
            CompressionMethod method, int level)
    {
        will_insert(std::string(path), input, method, level);
    }

    inline void will_remove(const std::string_view path)
    {
        will_remove(std::string(path));
    }

    inline void insert(const std::string_view path,
            CompressionMethod method, int level)
    {
        insert(std::string(path), method, level);
    }

    inline void insert(const std::string_view path, std::istream& input,
            CompressionMethod method, int level)
    {
        insert(std::string(path), input, method, level);
    }

    inline void remove(const std::string_view path)
    {
        remove(std::string(path));
    }

private:
    std::variant<std::iostream *, std::fstream> target;
    std::vector<InsertRequest> insert_sources;
    std::vector<std::string> paths_to_remove;
};

}
