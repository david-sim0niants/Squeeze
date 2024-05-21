#pragma once

#include <string_view>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include "compression_method.h"
#include "reader.h"

namespace squeeze {

class InsertRequest;

class Writer {
public:
    using WriteErrorCallback = std::function<void(const std::string_view path, Error<Writer> err)>;

    explicit Writer(std::iostream& target);

    void will_insert(std::string&& path, CompressionMethod method, int level);
    void will_insert(std::string&& path, std::istream& input, CompressionMethod method, int level);
    void will_remove(std::string&& path);

    void write();
    void write(const WriteErrorCallback& on_err);

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
    void perform_removes();
    void perform_inserts();
    void perform_inserts(const WriteErrorCallback& on_err);
    Error<Writer> perform_single_insert(InsertRequest& insert_request);

    std::iostream& target;
    Reader reader;
    std::unordered_map<std::string_view, InsertRequest> insert_requests;
    std::unordered_set<std::string> paths_to_remove;
};

}
