// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <istream>
#include <vector>

namespace squeeze::misc {

template<typename Char>
class BasicInputSubstreamIterator;

/* Some sort of container that binds to the given input stream and provides a size-limited view of it.
 * Reads from the substream will return EOF if read past the limit,
 * even if the original stream has more data available. */
template<typename Char>
class BasicInputSubstream {
public:
    using Iterator = BasicInputSubstreamIterator<Char>;

    explicit BasicInputSubstream(std::istream& stream, std::size_t size)
        : stream(stream), size(size), cache(0)
    {
    }

    /* Read one character. Return EOF in case of EOF. */
    int get();

    /* Get the begin iterator. */
    Iterator begin();
    /* Get the end iterator. */
    Iterator end();

    /* Return if EOF. */
    inline bool eof() const
    {
        return cache_off == cache.size() && (0 == size || stream.eof());
    }

private:
    std::istream& stream;
    std::size_t size;
    std::size_t cache_off = 0;
    std::vector<char> cache;

    static constexpr std::size_t cache_max_size = BUFSIZ;
};

/* Single-use iterator for traversing the substream. Acts like a std::istreambuf_iterator,
 * except it becomes invalid when reaching the substream EOF, NOT the original stream EOF. */
template<typename Char>
class BasicInputSubstreamIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = Char;
    using difference_type = std::ptrdiff_t;
    using pointer = Char *;
    using reference = Char&;

    using Substream = BasicInputSubstream<Char>;

    BasicInputSubstreamIterator() = default;
    explicit BasicInputSubstreamIterator(Substream& substream) : substream(&substream)
    {
    }

    inline BasicInputSubstreamIterator& operator++()
    {
        return *this;
    }

    inline BasicInputSubstreamIterator& operator++(int)
    {
        return *this;
    }

    inline value_type operator*() const
    {
        return substream->get();
    }

    inline bool operator==(const BasicInputSubstreamIterator& other) const
    {
        return substream == other.substream || !substream && other.substream->eof()
                                            || !other.substream && substream->eof();
    }

    inline bool operator!=(const BasicInputSubstreamIterator& other) const
    {
        return !(*this == other);
    }

private:
    Substream *substream = nullptr;
};

template<typename Char>
inline BasicInputSubstreamIterator<Char> BasicInputSubstream<Char>::begin()
{
    return Iterator{*this};
}

template<typename Char>
inline BasicInputSubstreamIterator<Char> BasicInputSubstream<Char>::end()
{
    return {};
}

using InputSubstream = BasicInputSubstream<char>;
using InputSubstreamIterator = BasicInputSubstreamIterator<char>;

}
