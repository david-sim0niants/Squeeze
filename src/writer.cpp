#include "squeeze/writer.h"

#include <variant>
#include <fstream>
#include <vector>
#include <filesystem>

#include "squeeze/exception.h"
#include "squeeze/utils/io.h"
#include "squeeze/utils/fs.h"

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

    inline bool has_custom_stream() const
    {
        return input;
    }

    inline ErrorCode open()
    {
        if (has_custom_stream())
            return success;

        *file_input = std::ifstream(path, std::ios_base::binary | std::ios_base::in);
        if (!*file_input)
            return std::make_error_code(static_cast<std::errc>(errno));

        input = &*file_input;
        return success;
    }

    inline void close()
    {
        if (file_input) {
            file_input.reset();
            input = nullptr;
        }
    }

    inline std::istream& get_stream()
    {
        return *input;
    }

    inline const std::istream& get_stream() const
    {
        return *input;
    }

private:
    std::string path;
    std::optional<std::ifstream> file_input;
    std::istream *input = nullptr;
    CompressionMethod compression_method = CompressionMethod::None;
    int compression_level = 0;
};

static constexpr EntryPermissions none_entry_type_permissions
            = EntryPermissions::OwnerRead | EntryPermissions::OwnerWrite
            | EntryPermissions::GroupRead | EntryPermissions::GroupWrite
            | EntryPermissions::OthersRead | EntryPermissions::OthersWrite;

static Error<Writer> make_entry_header(const InsertRequest& insert_request, EntryHeader& entry_header);


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
    perform_removes();
    target.seekp(0, std::ios_base::end);
    perform_inserts();
}

void Writer::write(const WriteErrorCallback& on_err)
{
    perform_removes();
    target.seekp(0, std::ios_base::end);
    perform_inserts(on_err);
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

void Writer::perform_removes()
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
}

void Writer::perform_inserts()
{
    for (auto& [path, insert_request] : insert_requests)
        perform_single_insert(insert_request);
}

void Writer::perform_inserts(const WriteErrorCallback& on_err)
{
    for (auto& [path, insert_request] : insert_requests)
        if (Error<Writer> e = perform_single_insert(insert_request))
            on_err(path, e);
}

Error<Writer> Writer::perform_single_insert(InsertRequest& insert_request)
{
    EntryHeader entry_header {};
    auto e = make_entry_header(insert_request, entry_header);
    if (e)
        return e;

    std::streampos initial_target_size = target.tellp();
    target.seekp(target.tellp() + static_cast<std::streamsize>(entry_header.get_header_size()));
    auto ec = insert_request.open();
    if (ec)
        return {"failed opening an input", ec.report()};

    switch (insert_request.get_compression_method()) {
        using enum CompressionMethod;
    case None:
        insert_request.get_stream().seekg(0, std::ios_base::end);
        entry_header.content_size = static_cast<uint64_t>(insert_request.get_stream().tellg());
        insert_request.get_stream().seekg(0);
        utils::ioscopy(insert_request.get_stream(), insert_request.get_stream().tellg(),
                       target, target.tellg(), entry_header.content_size);
        break;
    default:
        throw Exception<Writer>("invalid compression method");
    }

    if (insert_request.get_stream().fail() || target.fail())
        return "I/O failure";

    target.seekp(initial_target_size);
    auto ehe = EntryHeader::encode(target, entry_header);
    if (ehe)
        return {"failed encoding the entry header", ehe.report()};

    return success;
}


static Error<Writer> make_entry_header(const InsertRequest& insert_request, EntryHeader& entry_header)
{
    entry_header.content_size = 0;
    entry_header.compression_method = insert_request.get_compression_method();
    entry_header.compression_level = insert_request.get_compression_level();

    if (insert_request.has_custom_stream()) {
        entry_header.attributes.type = EntryType::None;
        entry_header.attributes.permissions = none_entry_type_permissions;
    } else {
        std::filesystem::path path(insert_request.get_path());
        std::filesystem::file_status st = std::filesystem::status(path);
        switch (st.type()) {
            using enum std::filesystem::file_type;
        case regular:
        case directory:
        case symlink:
            EntryType type;
            utils::convert(st.type(), type);
            EntryPermissions perms;
            utils::convert(st.permissions(), perms);
            entry_header.attributes.type = type;
            entry_header.attributes.permissions = perms;
            break;
        case not_found:
            return "no such file or directory in an insert request";
        case block:
            return "can't insert block file type";
        case character:
            return "can't insert character file type";
        case fifo:
            return "can't insert fifo file type";
        case socket:
            return "can't insert socket file type";
        case unknown:
            return "unknown file type";
        default:
            throw BaseException("Unexpected file type");
        }
    }

    entry_header.path = insert_request.get_path();
    entry_header.path_len = entry_header.path.size();

    return success;
}

}
