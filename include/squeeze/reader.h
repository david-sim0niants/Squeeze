#pragma once

#include <string_view>
#include <filesystem>
#include <vector>
#include <variant>
#include <fstream>

#include "entry.h"

namespace squeeze {

class ReaderIterator;

class Reader {
    friend class ReaderIterator;
public:
    explicit Reader(std::istream& source) : source(source)
    {}

    ReaderIterator begin() const;
    ReaderIterator end() const;

    void extract(std::string&& path);
    void extract(std::string&& path, std::ostream& output);

    inline void extract(const std::string_view path)
    {
        extract(std::string(path));
    }

    inline void extract(const std::string_view path, std::ostream& output)
    {
        extract(std::string(path), output);
    }

private:
    std::istream& source;
};


class ReaderIterator {
public:
    ReaderIterator& operator++();
    ReaderIterator operator++(int);

    inline const auto& operator*() const
    {
        return pos_and_entry_header;
    }

    inline const auto *operator->() const
    {
        return &pos_and_entry_header;
    }

    inline bool operator==(const ReaderIterator& other)
    {
        return &owner == &other.owner && pos_and_entry_header.first == other.pos_and_entry_header.first;
    }

    inline bool operator!=(const ReaderIterator& other)
    {
        return !(*this == other);
    }

    static constexpr uint64_t npos = uint64_t(-1);

private:
    friend class Reader;
    explicit ReaderIterator(const Reader& owner, bool begin);

    void read_current();

private:
    const Reader& owner;
    std::pair<uint64_t, EntryHeader> pos_and_entry_header;
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
