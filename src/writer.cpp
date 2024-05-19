#include "squeeze/writer.h"

namespace squeeze {

class InsertRequest {
private:
    using Input = std::variant<std::monostate, std::istream *, std::ifstream>;

public:
    InsertRequest(std::string&& path, CompressionMethod method, int level)
        : path(std::move(path)), compression_method(method), compression_level(level)
    {}

    InsertRequest(std::string&& path, std::istream& input, CompressionMethod method, int level)
        : path(std::move(path)), input(&input), compression_method(method), compression_level(level)
    {}

    inline const std::string_view get_path() const
    {
        return path;
    }

    inline const CompressionMethod get_compression_method() const
    {
        return compression_method;
    }

    inline const int get_compression_level() const
    {
        return compression_level;
    }

private:
    std::string path;
    Input input = std::monostate{};
    CompressionMethod compression_method = CompressionMethod::None;
    int compression_level = 0;
};

Writer::Writer(const char *path) : target(std::fstream(path))
{
}

Writer::Writer(const std::filesystem::path& path) : target(std::fstream(path))
{
}

Writer::Writer(std::iostream& target) : target(&target)
{
}

void Writer::will_insert(std::string&& path,
        CompressionMethod method, int level)
{
    insert_sources.emplace_back(std::move(path), method, level);
}

void Writer::will_insert(std::string&& path, std::istream& input,
        CompressionMethod method, int level)
{
    insert_sources.emplace_back(std::move(path), input, method, level);
}

void Writer::will_remove(std::string&& path)
{
    paths_to_remove.emplace_back(std::move(path));
}

void Writer::perform()
{
}

void Writer::insert(std::string&& path,
        CompressionMethod method, int level)
{
    will_insert(std::move(path), method, level);
    perform();
}

void Writer::insert(std::string&& path, std::istream& input,
        CompressionMethod method, int level)
{
    will_insert(std::move(path), input, method, level);
    perform();
}

}
