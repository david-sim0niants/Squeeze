#include "squeeze/reader.h"

#include <algorithm>

#include "squeeze/logging.h"
#include "squeeze/exception.h"
#include "squeeze/utils/io.h"

namespace squeeze {

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::Reader::"

ReaderIterator Reader::find_path(std::string_view path)
{
    return std::find_if(begin(), end(),
        [&path](const std::pair<uint64_t, EntryHeader>& pos_and_entry_header)
        {
            return pos_and_entry_header.second.path == path;
        });
}

Error<Reader> Reader::extract(const std::string_view path)
{
    auto it = find_path(path);
    return it == end() ? "non-existing path - " + std::string(path) : extract(it);
}

Error<Reader> Reader::extract(const std::string_view path, EntryHeader& header, std::ostream& output)
{
    auto it = find_path(path);
    header = it->second;
    return it == end() ? "non-existing path - " + std::string(path) : extract(it, output);
}

Error<Reader> Reader::extract(const ReaderIterator& it)
{
    FileEntryOutput entry_output;
    return extract(it, entry_output);
}

Error<Reader> Reader::extract(const ReaderIterator& it, std::ostream& output)
{
    CustomStreamEntryOutput entry_output(output);
    return extract(it, entry_output);
}

Error<Reader> Reader::extract(const ReaderIterator& it, EntryOutput& entry_output)
{
    SQUEEZE_TRACE("Extracting {}", it->second.path);

    auto& [pos, entry_header] = *it;
    source.seekg(pos + entry_header.get_encoded_header_size());

    SQUEEZE_DEBUG("Entry header: {}", utils::stringify(entry_header));

    switch (entry_header.attributes.type) {
        using enum EntryType;
    case None:
    case RegularFile:
    case Directory:
    {
        std::ostream *output;
        auto e = entry_output.init(entry_header, output);
        if (e) {
            SQUEEZE_ERROR("Failed initializing entry output");
            return {"failed initializing entry output", e.report()};
        }
        if (output) {
            return extract_plain(entry_header, *output);
        }
        break;
    }
    case Symlink:
    {
        std::string target;
        auto e = extract_symlink(entry_header, target);
        if (e) {
            SQUEEZE_ERROR("Failed extracting symlink");
            return {"failed extracting symlink", e.report()};
        }

        auto ee = entry_output.init_symlink(entry_header, target);
        if (ee) {
            SQUEEZE_ERROR("Failed extracting symlink");
            return {"failed extracting symlink", ee.report()};
        }

        break;
    }
    default:
        return Error<Reader>("invalid entry type");
    }
    return success;
}

bool Reader::is_corrupted() const
{
    source.seekg(0, std::ios_base::end);
    size_t size = source.tellg();
    ReaderIterator last_it = end();
    for (auto it = begin(); it != end(); ++it)
        last_it = it;
    return last_it == end() && size > 0
        || last_it->first + last_it->second.get_encoded_full_size() < size;
}

Error<Reader> Reader::extract_plain(const EntryHeader& entry_header, std::ostream& output)
{
    SQUEEZE_TRACE();

    switch (entry_header.compression.method) {
        using enum compression::CompressionMethod;
    case None:
        utils::ioscopy(source, source.tellg(), output, output.tellp(), entry_header.content_size);
        break;
    default:
        throw Exception<Reader>("invalid compression method");
    }

    if (utils::validate_stream_fail(source) or output.fail())
        return output.fail() ? "output writing failure" : "source reading failure";

    return success;
}

Error<Reader> Reader::extract_symlink(const EntryHeader& entry_header, std::string& target)
{
    SQUEEZE_TRACE();

    if (entry_header.content_size == 0) {
        SQUEEZE_ERROR("Symlink entry with no content");
        return "symlink entry with no content";
    }
    target.resize(static_cast<std::size_t>(entry_header.content_size) - 1);
    source.read(target.data(), target.size());
    if (source.fail())
        return "source reading failure";
    return success;
}

}
