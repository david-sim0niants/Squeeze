#pragma once

#include <cstdint>
#include <iterator>

#include "entry_header.h"

namespace squeeze {

class Reader;

class ReaderIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = std::pair<uint64_t, EntryHeader>;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type *;
    using reference = const value_type&;

    ReaderIterator& operator++() noexcept;
    ReaderIterator operator++(int) noexcept;

    inline reference operator*() const
    {
        return pos_and_entry_header;
    }

    inline pointer operator->() const
    {
        return &pos_and_entry_header;
    }

    inline bool operator==(const ReaderIterator& other) const noexcept
    {
        return pos_and_entry_header.first == other.pos_and_entry_header.first;
    }

    inline bool operator!=(const ReaderIterator& other) const noexcept
    {
        return pos_and_entry_header.first != other.pos_and_entry_header.first;
    }

    static constexpr uint64_t npos = uint64_t(-1);

private:
    friend class Reader;
    ReaderIterator()
        : owner(nullptr), pos_and_entry_header(npos, std::move(EntryHeader()))
    {}
    explicit ReaderIterator(const Reader& owner, bool begin);

    void read_current();

private:
    const Reader *owner;
    value_type pos_and_entry_header;
};

}
