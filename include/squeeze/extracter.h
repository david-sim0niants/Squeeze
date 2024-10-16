// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

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
    using Stat = StatStr;

    /* Construct the interface by passing a reference to the ostream source to read from. */
    explicit Extracter(std::istream& source) : source(source)
    {}

    /* Extract an entry from the given iterator to a file. */
    Stat extract(const EntryIterator& it);
    /* Extract an entry from the given iterator to the given custom output stream. */
    Stat extract(const EntryIterator& it, std::ostream& output);
    /* Extract an entry from the given iterator to the given entry output. */
    Stat extract(const EntryIterator& it, EntryOutput& entry_output);

protected:
    Stat extract_stream(const EntryHeader& entry_header, std::ostream& output);
    Stat extract_symlink(const EntryHeader& entry_header, std::string& target);

    std::istream& source;
};

}
