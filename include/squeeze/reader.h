#pragma once

#include <string_view>

#include "reader_iterator.h"
#include "entry_output.h"

namespace squeeze {

class Reader {
    friend class ReaderIterator;
public:
    explicit Reader(std::istream& source) : source(source)
    {}

    inline ReaderIterator begin() const
    {
        return ReaderIterator(*this, true);
    }

    inline const ReaderIterator& end() const
    {
        return end_it;
    }

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

    inline static const ReaderIterator end_it;
};

}
