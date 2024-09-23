#pragma once

#include <cassert>
#include <cstddef>

#include "squeeze/exception.h"

namespace squeeze::misc {

template<typename T>
class CircularIterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using reference = value_type&;
    using const_reference = const value_type&;

    CircularIterator() = default;

    CircularIterator(T *data, std::size_t size, std::size_t index = 0)
        : data(data), size(size), index(index)
    {
        if (0 == size)
            throw Exception<CircularIterator>("can't have 0 size");
        if (index >= size)
            throw Exception<CircularIterator>("index out of the range");
    }

    CircularIterator(const CircularIterator&) = default;
    CircularIterator& operator=(const CircularIterator&) = default;

    CircularIterator(CircularIterator&& other) noexcept
    {
        swap(other);
    }

    CircularIterator& operator=(CircularIterator&& other) noexcept
    {
        swap(other);
        return *this;
    }

    inline void swap(CircularIterator& other) noexcept
    {
        std::swap(data, other.data);
        std::swap(size, other.size);
        std::swap(index, other.index);
    }

    inline reference operator*() noexcept
    {
        return *get_ptr();
    }

    inline const_reference operator*() const noexcept
    {
        return *get_ptr();
    }

    inline T *operator->() noexcept
    {
        return get_ptr();
    }

    inline const T *operator->() const noexcept
    {
        return get_ptr();
    }

    inline CircularIterator& operator++() noexcept
    {
        index = (size == ++index ? 0 : index);
        return *this;
    }

    inline CircularIterator& operator++(int) noexcept
    {
        CircularIterator old = *this;
        ++*this;
        return old;
    }

    inline CircularIterator& operator--() noexcept
    {
        index = (0 == index-- ? size : index);
        return *this;
    }

    inline CircularIterator& operator--(int) noexcept
    {
        CircularIterator old = *this;
        --*this;
        return old;
    }

    inline CircularIterator& operator+=(std::ptrdiff_t off) noexcept
    {
        index = peek(+off);
        return *this;
    }

    inline CircularIterator& operator-=(std::ptrdiff_t off) noexcept
    {
        index = peek(-off);
        return *this;
    }

    inline CircularIterator operator+(std::ptrdiff_t off) const noexcept
    {
        CircularIterator tmp = *this;
        return tmp += off;
    }

    inline CircularIterator operator-(std::ptrdiff_t off) const noexcept
    {
        CircularIterator tmp = *this;
        return tmp -= off;
    }

    inline std::ptrdiff_t operator-(const CircularIterator& rhs) const noexcept
    {
        const T *lhs_ptr = get_ptr();
        const T *rhs_ptr = rhs.get_ptr();
        return lhs_ptr >= rhs_ptr ? (lhs_ptr - rhs_ptr) : (size - (rhs_ptr - lhs_ptr));
    }

    inline bool operator==(const CircularIterator& rhs) const noexcept
    {
        return get_ptr() == rhs.get_ptr();
    }

    inline bool operator!=(const CircularIterator& rhs) const noexcept
    {
        return get_ptr() != rhs.get_ptr();
    }

    inline bool operator<(const CircularIterator& rhs) const noexcept
    {
        return get_ptr() < rhs.get_ptr();
    }

    inline bool operator>(const CircularIterator& rhs) const noexcept
    {
        return get_ptr() > rhs.get_ptr();
    }

    inline bool operator<=(const CircularIterator& rhs) const noexcept
    {
        return get_ptr() <= rhs.get_ptr();
    }

    inline bool operator>=(const CircularIterator& rhs) const noexcept
    {
        return get_ptr() >= rhs.get_ptr();
    }

    inline T& operator[](std::ptrdiff_t off) noexcept
    {
        return data[peek(off)];
    }

    inline const T& operator[](std::ptrdiff_t off) const noexcept
    {
        return data[peek(off)];
    }

private:
    inline std::size_t peek(std::ptrdiff_t off) const noexcept
    {
        return off < 0 ? peek_left(-off) : peek_right(off);
    }

    inline std::size_t peek_right(std::size_t off) const noexcept
    {
        assert(off <= size);
        return (index + off) - (index >= size - off ? size : 0);
    }

    inline std::size_t peek_left(std::size_t off) const noexcept
    {
        assert(off <= size);
        return (index - off) + (index < off ? size : 0);
    }

    inline T *get_ptr() noexcept
    {
        return &data[index];
    }

    inline const T *get_ptr() const noexcept
    {
        return &data[index];
    }

    T *data = nullptr;
    std::size_t size = 0;
    std::size_t index = 0;
};

}
