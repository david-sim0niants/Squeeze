#include "squeeze/lister.h"

#include <algorithm>

namespace squeeze {

EntryIterator Lister::find(std::string_view path)
{
    return std::find_if(begin(), end(),
        [&path](const std::pair<uint64_t, EntryHeader>& pos_and_entry_header)
        {
            return pos_and_entry_header.second.path == path;
        });
}

bool Lister::is_corrupted() const
{
    source.seekg(0, std::ios_base::end);
    size_t size = source.tellg();
    EntryIterator last_it = end();
    for (auto it = begin(); it != end(); ++it)
        last_it = it;
    return last_it == end() && size > 0
        || last_it->first + last_it->second.get_encoded_full_size() < size;
}

}
