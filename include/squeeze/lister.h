#pragma once

#include <istream>
#include <string_view>

#include "squeeze/entry_iterator.h"

namespace squeeze {

/* Interface responsible for performing entry listing operations.
 * It behaves a bit like a container, providing begin() and end() iterators for iterating
 * over the entries using EntryIterator. */
class Lister {
public:
    explicit Lister(std::istream& source) : source(source)
    {
    }

    /* Iterator pointing to the first entry in the source. */
    inline EntryIterator begin() const
    {
        return EntryIterator(source);
    }

    /* Iterator pointing to the one past the last entry in the source. */
    inline const EntryIterator& end() const
    {
        return EntryIterator::end;
    }

    /* Find an entry iterator by path. */
    EntryIterator find(std::string_view path);

    /* Check if the source is corrupted. */
    bool is_corrupted() const;

protected:
    std::istream& source;
};

}
