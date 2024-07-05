#pragma once

#include <cstdint>
#include <iterator>
#include <istream>

#include "entry_header.h"

namespace squeeze {

class EntryIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = std::pair<uint64_t, EntryHeader>;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type *;
    using reference = const value_type&;

    explicit EntryIterator(std::istream& source);

    EntryIterator& operator++() noexcept;
    EntryIterator operator++(int) noexcept;

    inline reference operator*() const
    {
        return pos_and_entry_header;
    }

    inline pointer operator->() const
    {
        return &pos_and_entry_header;
    }

    inline bool operator==(const EntryIterator& other) const noexcept
    {
        return pos_and_entry_header.first == other.pos_and_entry_header.first;
    }

    inline bool operator!=(const EntryIterator& other) const noexcept
    {
        return pos_and_entry_header.first != other.pos_and_entry_header.first;
    }

    static constexpr uint64_t npos = uint64_t(-1);
    static const EntryIterator end;

private:
    EntryIterator() : source(nullptr), pos_and_entry_header(npos, std::move(EntryHeader()))
    {
    }

    void read_current();

private:
    std::istream *source;
    value_type pos_and_entry_header;
};

inline const EntryIterator EntryIterator::end {};

}
