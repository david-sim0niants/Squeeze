#include "squeeze/reader.h"

#include <algorithm>

#include "squeeze/exception.h"
#include "squeeze/utils/io.h"

namespace squeeze {

ReaderIterator Reader::find_path(std::string_view path)
{
    return std::find_if(begin(), end(),
        [&path](const std::pair<uint64_t, EntryHeader>& pos_and_entry_header)
        {
            return pos_and_entry_header.second.path == path;
        });
}

Error<Reader> Reader::extract(std::string&& path)
{
    auto it = find_path(path);
    return it == end() ? "non-existing path" : extract(it);
}

Error<Reader> Reader::extract(std::string&& path, EntryHeader& header, std::ostream& output)
{
    auto it = find_path(path);
    return it == end() ? "non-existing path" : extract(it, header, output);
}

Error<Reader> Reader::extract(const ReaderIterator& it)
{
    FileEntryOutput entry_output;
    return extract(it, entry_output);
}

Error<Reader> Reader::extract(const ReaderIterator& it, EntryHeader& header, std::ostream& output)
{
    CustomStreamEntryOutput entry_output(header, output);
    return extract(it, entry_output);
}

Error<Reader> Reader::extract(const ReaderIterator& it, EntryOutput& entry_output)
{
    auto& [pos, entry_header] = *it;
    source.seekg(pos + entry_header.get_header_size());

    switch (entry_header.attributes.type) {
        using enum EntryType;
    case None:
    case RegularFile:
    case Directory:
    {
        std::ostream *output;
        auto e = entry_output.init(entry_header, output);
        if (e)
            return {"failed initializing entry output", e.report()};
        if (output)
            return extract_plain(entry_header, *output);
        break;
    }
    case Symlink:
    {
        std::string target;
        auto e = extract_symlink(entry_header, target);
        if (e)
            return e;
        auto ee = entry_output.init_symlink(entry_header, target);
        if (ee)
            return ee.report();
        break;
    }
    default:
        throw Exception<Reader>("unexpected entry type");
    }

    return success;
}

Error<Reader> Reader::extract_plain(const EntryHeader& entry_header, std::ostream& output)
{
    switch (entry_header.compression_method) {
        using enum CompressionMethod;
    case None:
        utils::ioscopy(source, source.tellg(), output, output.tellp(), entry_header.content_size);
    default:
        throw Exception<Reader>("invalid compression method");
    }

    if (source.fail() || output.fail())
        return "I/O failure";

    return success;
}

Error<Reader> Reader::extract_symlink(const EntryHeader& entry_header, std::string& target)
{
    if (entry_header.content_size == 0)
        return "no content";
    target.resize(static_cast<std::size_t>(entry_header.content_size) - 1);
    source.read(target.data(), target.size());
    if (source.fail())
        return "I/O failure";
    return success;
}

}
