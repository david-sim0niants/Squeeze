#pragma once

#include <string_view>

#include "reader_iterator.h"
#include "entry_output.h"

namespace squeeze {

/* Interface responsible for performing entry listing and extracting operations.
 * It behaves a bit like a container, providing begin() and end() iterators for iterating
 * over the entries using the ReaderIterator.
 * Provides various extract methods that either immediately turn the provided entry into a file,
 * or fill the custom provided stream with the entry contents, or either expect an entry output
 * interface which is the generalized way of handling the entry extraction.
 * */
class Reader {
    friend class ReaderIterator;
public:
    /* Construct the interface by passing a reference to the ostream source to read from. */
    explicit Reader(std::istream& source) : source(source)
    {}

    /* Iterator pointing to the first entry in the source. */
    inline ReaderIterator begin() const
    {
        return ReaderIterator(*this, true);
    }

    /* Iterator pointing to the one past the last entry in the source. */
    inline const ReaderIterator& end() const
    {
        return end_it;
    }

    /* Find an entry iterator by path. */
    ReaderIterator find_path(std::string_view path);

    /* Extract an entry with the given path to a file. */
    Error<Reader> extract(const std::string_view path);
    /* Extract an entry with the given path to the given entry header and output stream. */
    Error<Reader> extract(const std::string_view path, EntryHeader& entry_header, std::ostream& output);

    /* Extract an entry from the given iterator to a file. */
    Error<Reader> extract(const ReaderIterator& it);
    /* Extract an entry from the given iterator to the given custom output stream. */
    Error<Reader> extract(const ReaderIterator& it, std::ostream& output);
    /* Extract an entry from the given iterator to the given entry output. */
    Error<Reader> extract(const ReaderIterator& it, EntryOutput& entry_output);

    /* Check if the source is corrupted. */
    bool is_corrupted() const;

private:
    Error<Reader> extract_plain(const EntryHeader& entry_header, std::ostream& output);
    Error<Reader> extract_symlink(const EntryHeader& entry_header, std::string& target);

    std::istream& source;

    inline static const ReaderIterator end_it;
};

}
