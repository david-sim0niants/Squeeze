#include "squeeze/entry_header.h"

#include <limits>

#include "squeeze/exception.h"
#include "squeeze/utils/io.h"

namespace squeeze {

namespace {

template<typename T>
StatStr encode_any(std::ostream& output, const T& obj)
{
    output.write(reinterpret_cast<const char *>(&obj), sizeof(obj));
    if (utils::validate_stream_fail_eof(output)) [[unlikely]]
        return "output write error";
    else
        return success;
}

template<typename T>
StatStr decode_any(std::istream& input, T& obj)
{
    std::errc err;
    input.read(reinterpret_cast<char *>(&obj), sizeof(obj));
    if (utils::validate_stream_fail_eof(input)) [[unlikely]]
        return "input read error";
    else
        return success;
}

StatStr encode_path(std::ostream& output, const std::string& path)
{
    static constexpr size_t path_len_limit =
        std::numeric_limits<EntryHeader::EncodedPathSizeType>::max();

    if (path.size() > path_len_limit)
        throw Exception<EntryHeader>("path too long, must not exceed " + stringify(path_len_limit));

    StatStr s = encode_any(output, static_cast<EntryHeader::EncodedPathSizeType>(path.size()));
    if (s.failed())
        return s;

    output.write(path.data(), path.size());

    if (utils::validate_stream_fail_eof(output)) [[unlikely]]
        return "output write error";
    else
        return success;
}

StatStr decode_path(std::istream& input, std::string& path)
{
    EntryHeader::EncodedPathSizeType size;
    StatStr s = decode_any(input, size);
    if (s.failed()) [[unlikely]]
        return s;

    path.resize(size);
    input.read(path.data(), path.size());

    if (utils::validate_stream_fail_eof(input)) [[unlikely]]
        return "input read error";
    else
        return success;
}

}

StatStr EntryHeader::encode_content_size(std::ostream &output, uint64_t content_size)
{
    output.seekp(output.tellp() + static_cast<std::streamoff>(sizeof(EntryHeader::major_minor_version)));
    return encode_any(output, content_size);
}

StatStr EntryHeader::decode_content_size(std::istream &input, uint64_t& content_size)
{
    input.seekg(input.tellg() + static_cast<std::streamoff>(sizeof(EntryHeader::major_minor_version)));
    return decode_any(input, content_size);
}

StatStr EntryHeader::encode(std::ostream& output, const EntryHeader& entry_header)
{
    switch (entry_header.compression.method) {
        using enum compression::CompressionMethod;
    case None:
    case Huffman:
    case Deflate:
        break;
    default: [[unlikely]]
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

    StatStr s = success;
    (s = encode_any(output, entry_header.major_minor_version)) &&
    (s = encode_any(output, entry_header.content_size)) &&
    (s = encode_any(output, entry_header.compression)) &&
    (s = encode_any(output, entry_header.attributes)) &&
    (s = encode_path(output, entry_header.path));
    return s;
}

StatStr EntryHeader::decode(std::istream& input, EntryHeader& entry_header)
{
    StatStr s = success;
    (s = decode_any(input, entry_header.major_minor_version)) &&
    (s = decode_any(input, entry_header.content_size)) &&
    (s = decode_any(input, entry_header.compression)) &&
    (s = decode_any(input, entry_header.attributes)) &&
    (s = decode_path(input, entry_header.path));
    if (s.failed()) [[unlikely]]
        return s;

    switch (entry_header.compression.method) {
        using enum compression::CompressionMethod;
    case None:
    case Huffman:
    case Deflate:
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
    default: [[unlikely]]
        return "invalid entry type";
    }

    return success;
}

template<> void print_to(std::ostream& os, const EntryHeader& header)
{
    print_to(os,
        "{ content_size=", header.content_size,
        ", major_minor_version=", header.major_minor_version.major, '.', header.major_minor_version.minor,
        ", compression=", header.compression,
        ", attributes=", header.attributes,
        ", path=", header.path, " }");
}

}
