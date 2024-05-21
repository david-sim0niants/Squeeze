#include "squeeze/writer.h"

#include <variant>
#include <fstream>
#include <vector>

#include "squeeze/utils/io.h"

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

Writer::Writer(std::iostream& target) : target(target), reader(target)
{
}

void Writer::will_insert(std::string&& path,
        CompressionMethod method, int level)
{
    insert_requests.emplace(std::piecewise_construct,
        std::forward_as_tuple(path), std::forward_as_tuple(std::move(path), method, level));
}

void Writer::will_insert(std::string&& path, std::istream& input,
        CompressionMethod method, int level)
{
    insert_requests.emplace(std::piecewise_construct,
        std::forward_as_tuple(path), std::forward_as_tuple(std::move(path), input, method, level));
}

void Writer::will_remove(std::string&& path)
{
    auto it = insert_requests.find(path);
    if (it != insert_requests.end())
        insert_requests.erase(it);
    paths_to_remove.emplace(std::move(path));
}

void Writer::write()
{
    std::vector<std::pair<uint64_t, uint64_t>> holes;
    for (auto it = reader.begin(); it != reader.end(); ++it) {
        auto& [pos, entry_header] = *it;
        if (!paths_to_remove.contains(entry_header.path) && !insert_requests.contains(entry_header.path))
            continue;
        holes.emplace_back(pos, entry_header.get_total_size());
    }
    target.seekg(0, std::ios_base::end);
    uint64_t stream_end = static_cast<uint64_t>(target.tellg());
    holes.emplace_back(stream_end, ReaderIterator::npos);

    size_t tot_hole = 0;
    for (size_t i = 1; i < holes.size(); ++i) {
        tot_hole += holes[i - 1].second;
        uint64_t non_hole_beg = holes[i - 1].first + holes[i - 1].second;
        uint64_t non_hole_end = holes[i].first;

        utils::iosmove(target, non_hole_beg - tot_hole, non_hole_beg, non_hole_end - non_hole_beg);
    }

    // TODO: insertion
}

void Writer::insert(std::string&& path,
        CompressionMethod method, int level)
{
    will_insert(std::move(path), method, level);
    write();
}

void Writer::insert(std::string&& path, std::istream& input,
        CompressionMethod method, int level)
{
    will_insert(std::move(path), input, method, level);
    write();
}

}
