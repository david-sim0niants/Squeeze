#include "squeeze/reader_iterator.h"
#include "squeeze/reader.h"

namespace squeeze {

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
    pos_and_entry_header.first += pos_and_entry_header.second.get_full_size();
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
