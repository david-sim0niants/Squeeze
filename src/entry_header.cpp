#include "squeeze/entry_header.h"
#include "squeeze/exception.h"

#include <limits>

namespace squeeze {

namespace {

template<typename T>
Error<EntryHeader> encode_any(std::ostream& output, const T& obj)
{
    output.write(reinterpret_cast<const char *>(&obj), sizeof(obj));
    if (output.fail()) {
        output.clear();
        return "output write error";
    }
    return success;
}

template<typename T>
Error<EntryHeader> decode_any(std::istream& input, T& obj)
{
    input.read(reinterpret_cast<char *>(&obj), sizeof(obj));
    if (input.fail()) {
        input.clear();
        return "input read error";
    }
    return success;
}

Error<EntryHeader> encode_path(std::ostream& output, const std::string& path)
{
    static constexpr size_t path_len_limit =
        std::numeric_limits<EntryHeader::EncodedPathSizeType>::max();

    if (path.size() > path_len_limit)
        throw Exception<EntryHeader>("path too long, must not exceed " + utils::stringify(path_len_limit));

    if (auto e = encode_any(output, static_cast<EntryHeader::EncodedPathSizeType>(path.size())))
        return e;
    output.write(path.data(), path.size());
    if (output.fail()) {
        output.clear();
        return "output write error";
    }
    return success;
}

Error<EntryHeader> decode_path(std::istream& input, std::string& path)
{
    EntryHeader::EncodedPathSizeType size;
    if (auto e = decode_any(input, size))
        return e;

    path.resize(size);
    input.read(path.data(), path.size());

    if (input.fail()) {
        input.clear();
        return "input read error";
    }
    return success;
}

}

Error<EntryHeader> EntryHeader::encode_content_size(std::ostream &output, uint64_t content_size)
{
    output.seekp(output.tellp() + static_cast<std::streamoff>(sizeof(EntryHeader::major_minor_version)));
    return encode_any(output, content_size);
}

Error<EntryHeader> EntryHeader::decode_content_size(std::istream &input, uint64_t& content_size)
{
    input.seekg(input.tellg() + static_cast<std::streamoff>(sizeof(EntryHeader::major_minor_version)));
    return decode_any(input, content_size);
}

Error<EntryHeader> EntryHeader::encode(std::ostream& output, const EntryHeader& entry_header)
{
    switch (entry_header.compression.method) {
        using enum compression::CompressionMethod;
    case None:
        break;
    default:
        throw Exception<EntryHeader>("invalid compression method");
    }

    switch (entry_header.attributes.type) {
        using enum EntryType;
    case None:
    case RegularFile:
    case Directory:
    case Symlink:
        break;
    default:
        throw Exception<EntryHeader>("invalid entry type");
    }

    Error<EntryHeader> e = success;
    if (    (e = encode_any(output, entry_header.major_minor_version)).successful()
        and (e = encode_any(output, entry_header.content_size)).successful()
        and (e = encode_any(output, entry_header.compression)).successful()
        and (e = encode_any(output, entry_header.attributes)).successful()
        and (e = encode_path(output, entry_header.path)).successful()   )
        return success;
    else
        return e;
}

Error<EntryHeader> EntryHeader::decode(std::istream& input, EntryHeader& entry_header)
{
    Error<EntryHeader> e = success;
    if (   (e = decode_any(input, entry_header.major_minor_version)).failed()
        or (e = decode_any(input, entry_header.content_size)).failed()
        or (e = decode_any(input, entry_header.compression)).failed()
        or (e = decode_any(input, entry_header.attributes)).failed()
        or (e = decode_path(input, entry_header.path)).failed()         )
        return e;

    switch (entry_header.compression.method) {
        using enum compression::CompressionMethod;
    case None:
        break;
    default:
        return "invalid compression method";
    }

    switch (entry_header.attributes.type) {
    case EntryType::None:
    case EntryType::RegularFile:
    case EntryType::Directory:
    case EntryType::Symlink:
        break;
    default:
        return "invalid entry type";
    }

    return success;
}

template<> std::string utils::stringify(const EntryHeader& header)
{
    return  "{ content_size=" + utils::stringify(header.content_size) +
            ", compression=" + utils::stringify(header.compression) +
            ", attributes=" + utils::stringify(header.attributes) +
            ", path=" + header.path + " }";
}

}
