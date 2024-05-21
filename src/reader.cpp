#include "squeeze/reader.h"

#include <algorithm>

#include "squeeze/exception.h"
#include "squeeze/utils/fs.h"

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

Error<Reader> Reader::extract(std::string&& path, std::ostream& output)
{
    auto it = find_path(path);
    return it == end() ? "non-existing path" : extract(it, output);
}

Error<Reader> Reader::extract(const ReaderIterator& it)
{
    const auto& [pos, entry_header] = *it;
    switch (entry_header.attributes.type) {
    case EntryType::None:
        return "cannot extract none type entry without a custom output stream";
    case EntryType::RegularFile:
        {
            auto result = utils::make_regular_file(entry_header.path, entry_header.attributes.permissions);
            if (auto *file = std::get_if<std::fstream>(&result))
                return extract(it, *file);
            else
                return get<ErrorCode>(result).report();
        }
    case EntryType::Directory:
        {
            auto e = utils::make_directory(entry_header.path, entry_header.attributes.permissions);
            if (e.failed())
                return e.report();
            else
                return {};
        }
    case EntryType::Symlink:
        {
            std::stringstream output;
            Error<Reader> e = extract(it, output);
            if (e.failed())
                return e;

            ErrorCode ec = utils::make_symlink(entry_header.path, output.str(),
                    entry_header.attributes.permissions);
            if (ec.failed())
                return ec.report();
            else
                return {};
        }
    default:
        throw Exception<Reader>("attempt to extract an entry with an invalid type");
    }
}

Error<Reader> Reader::extract(const ReaderIterator& it, std::ostream& output)
{
    // TODO: extraction
    return {};
}


ReaderIterator::ReaderIterator(const Reader& owner, bool begin)
    : owner(&owner), pos_and_entry_header(npos, EntryHeader())
{
    if (!begin)
        return;

    pos_and_entry_header.first = 0;
    read_current();
}

ReaderIterator& ReaderIterator::operator++() noexcept
{
    pos_and_entry_header.first += pos_and_entry_header.second.get_total_size();
    read_current();
    return *this;
}

ReaderIterator ReaderIterator::operator++(int) noexcept
{
    ReaderIterator copied = *this;
    ++*this;
    return copied;
}

void ReaderIterator::read_current()
{
    owner->source.seekg(pos_and_entry_header.first);
    if (EntryHeader::decode(owner->source, pos_and_entry_header.second).failed()) {
        pos_and_entry_header.first = npos;
        owner->source.clear();
    }
}

}
