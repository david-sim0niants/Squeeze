#pragma once

#include <utility>
#include <iterator>
#include <algorithm>

namespace squeeze::utils {

template<std::invocable F> requires (not std::is_void_v<std::invoke_result_t<F>>)
class FunctionInputIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = std::invoke_result_t<F>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type *;
    using reference = value_type&;

    explicit FunctionInputIterator(F f) : f(std::move(f)) {}

    constexpr inline value_type operator*() const
    {
        return f();
    }

    constexpr inline FunctionInputIterator& operator++()
    {
        return *this;
    }

    constexpr inline FunctionInputIterator& operator++(int)
    {
        return *this;
    }

private:
    F f;
};

template<typename F>
class FunctionOutputIterator {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;

    explicit FunctionOutputIterator(F f) : f(f) {}

    template<typename Arg>
    constexpr FunctionOutputIterator& operator=(Arg&& arg)
    {
        f(std::forward<decltype(arg)>(arg));
        return *this;
    }

    constexpr inline FunctionOutputIterator& operator*()
    {
        return *this;
    }

    constexpr inline FunctionOutputIterator& operator++()
    {
        return *this;
    }

    constexpr inline FunctionOutputIterator& operator++(int)
    {
        return *this;
    }

private:
    F f;
};

}
