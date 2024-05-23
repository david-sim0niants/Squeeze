#include "squeeze/entry_header.h"

namespace squeeze {

Error<EntryHeader> EntryHeader::encode(std::ostream& output, const EntryHeader& entry_header)
{
    output  << entry_header.content_size
            << static_cast<std::underlying_type_t<CompressionMethod>>(entry_header.compression_method)
            << entry_header.compression_level
            << entry_header.attributes
            << entry_header.path_len
            << entry_header.path;
    if (output.fail())
        return "output write error";
    return success;
}

Error<EntryHeader> EntryHeader::decode(std::istream& input, EntryHeader& entry_header)
{
    std::underlying_type_t<CompressionMethod> compression_method_value;
    input   >> entry_header.content_size
            >> compression_method_value
            >> entry_header.compression_level
            >> entry_header.attributes
            >> entry_header.path_len;

    if (input.fail())
        return "input read error";

    entry_header.compression_method = static_cast<CompressionMethod>(compression_method_value);
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

    entry_header.path.resize(entry_header.path_len);
    input.getline(entry_header.path.data(), entry_header.path_len, '\0');
    if (input.fail())
        return "failed parsing a path";

    return success;
}

}
