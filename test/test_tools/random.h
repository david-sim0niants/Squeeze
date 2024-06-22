#pragma once

#include <concepts>
#include <random>

namespace squeeze::testing::tools {

template<std::integral Integral>
class Random {
public:
    Random() : generator(std::random_device{}()) {}
    explicit Random(std::mt19937::result_type seed) : generator(seed) {}

    inline Integral operator()(Integral min, Integral max) const
    {
        return std::uniform_int_distribution<Integral>{min, max}(generator);
    }

private:
    mutable std::mt19937 generator;
};

std::string generate_random_string(const Random<int>& prng, size_t size);

}
