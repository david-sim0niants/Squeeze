#pragma once

#include <cassert>
#include <cstddef>

#include "squeeze/exception.h"

namespace squeeze::misc {

template<typename T, std::size_t size>
    requires (size > 0)
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

    CircularIterator(T *data, std::size_t index = 0) : data(data), index(index)
    {
        if (index >= size)
            throw Exception<CircularIterator>("index out of the range");
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
        ++index %= size;
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
        --index %= size;
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
        index = peek(off);
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
        return (get_ptr() - rhs.get_ptr()) % size;
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
        return (index + off) % size;
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
    std::size_t index = 0;
};

}
