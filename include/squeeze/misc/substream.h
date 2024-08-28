#pragma once

#include <istream>
#include <vector>

namespace squeeze::misc {

template<typename Char>
class BasicInputSubstreamIterator;

template<typename Char>
class BasicInputSubstream {
public:
    using Iterator = BasicInputSubstreamIterator<Char>;

    explicit BasicInputSubstream(std::istream& stream, std::size_t size)
        : stream(stream), size(size), cache(0)
    {
    }

    int get();

    Iterator begin();
    Iterator end();

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
