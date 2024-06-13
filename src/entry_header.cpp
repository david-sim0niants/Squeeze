#include "squeeze/entry_header.h"

namespace squeeze {

Error<EntryHeader> EntryHeader::encode(std::ostream& output, const EntryHeader& entry_header)
{
    output.write(reinterpret_cast<const char *>(&entry_header), static_size);
    output.write(entry_header.path.data(), entry_header.path_len);
    if (output.fail())
        return "output write error";
    return success;
}

Error<EntryHeader> EntryHeader::decode(std::istream& input, EntryHeader& entry_header)
{
    input.read(reinterpret_cast<char *>(&entry_header), static_size);
    if (input.fail())
        return "input read error";

    entry_header.path.resize(entry_header.path_len);
    input.read(entry_header.path.data(), entry_header.path_len);
    if (input.fail())
        return "failed parsing a path";

    switch (entry_header.compression_method) {
    case CompressionMethod::None:
        break;
    default:
        return "invalid compression method parsed";
    }

    switch (entry_header.attributes.type) {
    case EntryType::None:
    case EntryType::RegularFile:
    case EntryType::Directory:
    case EntryType::Symlink:
        break;
    default:
        return "invalid entry type parsed";
    }

    return success;
}

template<> std::string utils::stringify(const EntryHeader& header)
{
    const char *compression_method = "[unknown]";
    switch (header.compression_method) {
    case CompressionMethod::None:
        compression_method = "none";
        break;
    default:
        break;
    }

    return  "{ content_size=" + utils::stringify(header.content_size) +
            ", compression_method=" + compression_method +
            ", compression_level=" + utils::stringify(header.compression_level) +
            ", attributes=" + utils::stringify(header.attributes) +
            ", path=" + header.path + " }";
}

}
