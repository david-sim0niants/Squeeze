// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "squeeze/entry_iterator.h"

namespace squeeze {

EntryIterator::EntryIterator(std::istream& source)
    : source(&source), pos_and_entry_header(npos, EntryHeader())
{
    pos_and_entry_header.first = 0;
    read_current();
}

EntryIterator& EntryIterator::operator++() noexcept
{
    pos_and_entry_header.first += pos_and_entry_header.second.get_encoded_full_size();
    read_current();
    return *this;
}

EntryIterator EntryIterator::operator++(int) noexcept
{
    EntryIterator copied = *this;
    ++*this;
    return copied;
}

void EntryIterator::read_current()
{
    source->seekg(pos_and_entry_header.first);
    if (EntryHeader::decode(*source, pos_and_entry_header.second).failed()) {
        pos_and_entry_header.first = npos;
        source->clear();
    }
}

}
