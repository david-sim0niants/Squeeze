#include "squeeze/reader.h"

namespace squeeze {

class ExtractRequest {
private:
    using Output = std::variant<std::monostate, std::ostream *, std::ofstream>;

public:
    ExtractRequest(std::string&& path)
        : path(std::move(path))
    {}

    ExtractRequest(std::string&& path, std::ostream& output)
        : path(std::move(path)), output(&output)
    {}

    inline const std::string_view get_path() const
    {
        return path;
    }

private:
    const std::string_view path;
    Output output = std::monostate{};
};


void Reader::extract(std::string&& path)
{
}

void Reader::extract(std::string&& path, std::ostream& output)
{
}


ReaderIterator::ReaderIterator(const Reader& owner, bool begin)
    : owner(owner), pos_and_entry_header(npos, EntryHeader())
{
    if (!begin)
        return;

    pos_and_entry_header.first = 0;
    read_current();
}

ReaderIterator& ReaderIterator::operator++()
{
    pos_and_entry_header.first +=
        (EntryHeader::static_size + pos_and_entry_header.second.path_len
         + pos_and_entry_header.second.content_size);
    read_current();
    return *this;
}

ReaderIterator ReaderIterator::operator++(int)
{
    ReaderIterator copied = *this;
    ++*this;
    return copied;
}

void ReaderIterator::read_current()
{
    owner.source.seekg(pos_and_entry_header.first);
    if (EntryHeader::decode(owner.source, pos_and_entry_header.second).failed())
        pos_and_entry_header.first = npos;
}

}
