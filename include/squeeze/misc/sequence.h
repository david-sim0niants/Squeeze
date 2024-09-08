#pragma once

namespace squeeze::misc {

/* Abstraction for handling two types of sequences: bounded and unbounded.
 *
 * Bounded sequences use two iterators: one for the current position and the second for the end.
 * Unbounded sequences use only one iterator and are always considered valid.
 * Therefore, the is_valid() method is always true for unbounded sequences,
 * while for bounded sequences, it is true only if the current iterator is not equal to the end iterator. */
template<typename It, typename ItEnd = void>
struct Sequence;

template<typename It>
struct UnboundedSequenceEndIt {
    inline bool operator==(It) const noexcept
    {
        return false;
    }

    inline bool operator!=(It) const noexcept
    {
        return true;
    }

    friend bool operator==(const It&, UnboundedSequenceEndIt<It>) noexcept
    {
        return false;
    }

    friend bool operator!=(const It&, UnboundedSequenceEndIt<It>) noexcept
    {
        return true;
    }
};

template<typename It> struct Sequence<It, void> {
    explicit Sequence(It it) : it(it)
    {
    }

    inline bool is_valid() const noexcept
    {
        return true;
    }

    inline It& get_iter() noexcept
    {
        return it;
    }

    inline const It& get_iter() const noexcept
    {
        return it;
    }

    inline It begin() const noexcept
    {
        return it;
    }

    inline UnboundedSequenceEndIt<It> end()
    {
        return {};
    }

    static constexpr bool bounded = false;

    It it;
};

template<typename It> struct Sequence<It, It> {
    explicit Sequence(It it, It it_end) : it(it), it_end(it_end)
    {
    }

    inline bool is_valid() const noexcept
    {
        return it != it_end;
    }

    inline It begin() const noexcept
    {
        return it;
    }

    inline It end() const noexcept
    {
        return it_end;
    }

    static constexpr bool bounded = true;

    It it, it_end;
};

template<typename It> Sequence(It) -> Sequence<It>;
template<typename It> Sequence(It, It) -> Sequence<It, It>;

}
