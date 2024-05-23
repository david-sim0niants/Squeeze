#pragma once

#include <string_view>
#include <iterator>

#include "entry_output.h"

namespace squeeze {

class ReaderIterator;

class Reader {
    friend class ReaderIterator;
public:
    explicit Reader(std::istream& source) : source(source)
    {}

    ReaderIterator begin() const;
    ReaderIterator end() const;

    ReaderIterator find_path(std::string_view path);

    Error<Reader> extract(std::string&& path);
    Error<Reader> extract(std::string&& path, EntryHeader& header, std::ostream& output);

    inline Error<Reader> extract(const std::string_view path)
    {
        return extract(std::string(path));
    }

    inline Error<Reader> extract(const std::string_view path, std::ostream& output)
    {
        return extract(std::string(path), output);
    }

    Error<Reader> extract(const ReaderIterator& it);
    Error<Reader> extract(const ReaderIterator& it, EntryHeader& entry_header, std::ostream& output);
    Error<Reader> extract(const ReaderIterator& it, EntryOutput& entry_output);

private:
    Error<Reader> extract_plain(const EntryHeader& entry_header, std::ostream& output);
    Error<Reader> extract_symlink(const EntryHeader& entry_header, std::string& target);

    std::istream& source;
};


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
        return owner == other.owner && pos_and_entry_header.first == other.pos_and_entry_header.first;
    }

    inline bool operator!=(const ReaderIterator& other) const noexcept
    {
        return owner != other.owner || pos_and_entry_header.first != other.pos_and_entry_header.first;
    }

    static constexpr uint64_t npos = uint64_t(-1);

private:
    friend class Reader;
    explicit ReaderIterator(const Reader& owner, bool begin);

    void read_current();

private:
    const Reader *owner;
    value_type pos_and_entry_header;
};


inline ReaderIterator Reader::begin() const
{
    return ReaderIterator(*this, true);
}

inline ReaderIterator Reader::end() const
{
    return ReaderIterator(*this, false);
}

}
