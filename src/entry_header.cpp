#include "squeeze/entry_header.h"

#include <limits>
#include <algorithm>

#include "squeeze/exception.h"
#include "squeeze/utils/io.h"
#include "squeeze/utils/endian.h"

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
    input.read(reinterpret_cast<char *>(&obj), sizeof(obj));
    if (utils::validate_stream_fail_eof(input)) [[unlikely]]
        return "input read error";
    else
        return success;
}

template<std::integral T>
StatStr encode_integral(std::ostream& output, T val)
{
    return encode_any(output, utils::to_endian_val<std::endian::little>(val));
}

template<std::integral T>
StatStr decode_integral(std::istream& input, T& val)
{
    StatStr stat = decode_any(input, val);
    val = utils::from_endian_val<std::endian::little>(val);
    return stat;
}

StatStr encode_compression_params(std::ostream& output, const CompressionParams& params)
{
    using compression::CompressionMethod;
    switch (params.method) {
        using enum CompressionMethod;
    case None:
    case Huffman:
    case Deflate:
        break;
    default: [[unlikely]]
        throw Exception<EntryHeader>("invalid compression method");
    }

    const auto method_int = static_cast<std::underlying_type_t<CompressionMethod>>(params.method);
    StatStr s;
    (s = encode_integral(output, method_int)) &&
    (s = encode_integral(output, params.level));
    return s;
}

StatStr decode_compression_params(std::istream& input, CompressionParams& params)
{
    using compression::CompressionMethod;
    std::underlying_type_t<CompressionMethod> method_int {};
    StatStr s;
    (s = decode_integral(input, method_int)) &&
    (s = decode_integral(input, params.level));

    params.method = static_cast<CompressionMethod>(method_int);

    switch (params.method) {
        using enum CompressionMethod;
    case None:
    case Huffman:
    case Deflate:
        break;
    default: [[unlikely]]
        if (s)
            s = "invalid compression method";
        params.method = CompressionMethod::Unknown;
        break;
    }

    return s;
}

StatStr encode_entry_attributes(std::ostream& output, EntryAttributes attributes)
{
    switch (attributes.get_type()) {
        using enum EntryType;
    case None:
    case RegularFile:
    case Directory:
    case Symlink:
        break;
    default: [[unlikely]]
        throw Exception<EntryHeader>("invalid entry type");
    }
    return encode_integral(output, attributes.data);
}

StatStr decode_entry_attributes(std::istream& input, EntryAttributes& attributes)
{
    StatStr s = decode_integral(input, attributes.data);
    switch (attributes.get_type()) {
    case EntryType::None:
    case EntryType::RegularFile:
    case EntryType::Directory:
    case EntryType::Symlink:
        break;
    default: [[unlikely]]
        if (s)
            s = "invalid entry type";
        attributes.set_type(EntryType::None);
        break;
    }
    return s;
}

StatStr encode_path(std::ostream& output, const std::string& path)
{
    static constexpr size_t path_size_limit =
        std::numeric_limits<EntryHeader::EncodedPathSizeType>::max();

    if (path.size() > path_size_limit)
        throw Exception<EntryHeader>("path too long, must not exceed " + stringify(path_size_limit));

    const auto path_size = static_cast<EntryHeader::EncodedPathSizeType>(path.size());
    StatStr s = encode_integral(output, path_size);
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
    EntryHeader::EncodedPathSizeType path_size = 0;
    StatStr s = decode_integral(input, path_size);
    if (s.failed()) [[unlikely]]
        return s;

    path.resize(path_size);
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
    return encode_integral(output, content_size);
}

StatStr EntryHeader::decode_content_size(std::istream &input, uint64_t& content_size)
{
    input.seekg(input.tellg() + static_cast<std::streamoff>(sizeof(EntryHeader::major_minor_version)));
    return decode_integral(input, content_size);
}

StatStr EntryHeader::encode(std::ostream& output, const EntryHeader& entry_header)
{
    StatStr s;
    (s = encode_integral(output, entry_header.major_minor_version.major)) &&
    (s = encode_integral(output, entry_header.major_minor_version.minor)) &&
    (s = encode_integral(output, entry_header.content_size)) &&
    (s = encode_compression_params(output, entry_header.compression)) &&
    (s = encode_entry_attributes(output, entry_header.attributes)) &&
    (s = encode_path(output, entry_header.path));
    return s;
}

StatStr EntryHeader::decode(std::istream& input, EntryHeader& entry_header)
{
    StatStr s;
    (s = decode_integral(input, entry_header.major_minor_version.major)) &&
    (s = decode_integral(input, entry_header.major_minor_version.minor)) &&
    (s = decode_integral(input, entry_header.content_size)) &&
    (s = decode_compression_params(input, entry_header.compression)) &&
    (s = decode_entry_attributes(input, entry_header.attributes)) &&
    (s = decode_path(input, entry_header.path));
    return s;
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
