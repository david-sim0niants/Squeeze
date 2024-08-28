#include "squeeze/misc/substream.h"

namespace squeeze::misc {

template<typename Char>
int BasicInputSubstream<Char>::get()
{
    if (cache_off == cache.size()) [[unlikely]] {
        if (0 == size) [[unlikely]]
            return EOF;
        cache_off = 0;
        cache.resize(std::min(size, cache_max_size));
        size -= cache.size();
        stream.read(cache.data(), cache.size());
        cache.resize(stream.gcount());
        if (cache.size() == 0)
            return EOF;
    }
    return cache[cache_off++];
}

template class BasicInputSubstream<char>;
template class BasicInputSubstreamIterator<char>;

}
