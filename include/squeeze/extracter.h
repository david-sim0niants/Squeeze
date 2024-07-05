#pragma once

#include "entry_iterator.h"
#include "entry_output.h"

namespace squeeze {

/* Interface responsible for performing entry extracting operations.
 * Provides various extract methods that either immediately turn the provided entry into a file,
 * or fill the custom provided stream with the entry contents, or either expect an entry output
 * interface which is the generalized way of handling the entry extraction.
 * */
class Extracter {
public:
    /* Construct the interface by passing a reference to the ostream source to read from. */
    explicit Extracter(std::istream& source) : source(source)
    {}

    /* Extract an entry from the given iterator to a file. */
    Error<Extracter> extract(const EntryIterator& it);
    /* Extract an entry from the given iterator to the given custom output stream. */
    Error<Extracter> extract(const EntryIterator& it, std::ostream& output);
    /* Extract an entry from the given iterator to the given entry output. */
    Error<Extracter> extract(const EntryIterator& it, EntryOutput& entry_output);

protected:
    Error<Extracter> extract_plain(const EntryHeader& entry_header, std::ostream& output);
    Error<Extracter> extract_symlink(const EntryHeader& entry_header, std::string& target);

    std::istream& source;
};

}
