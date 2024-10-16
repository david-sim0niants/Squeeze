// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <concepts>
#include <random>

namespace squeeze::test_tools::generators {

class PRNG {
public:
    using SeedType = std::mt19937::result_type;

    PRNG() : generator(std::random_device{}()) {}
    explicit PRNG(SeedType seed) : generator(seed) {}

    template<std::integral Integral = int>
    inline Integral gen(Integral min, Integral max) const
    {
        return std::uniform_int_distribution<Integral>{min, max}(generator);
    }

    template<std::integral Integral = int>
    inline Integral operator()(Integral min, Integral max) const
    {
        return gen<Integral>(min, max);
    }

private:
    mutable std::mt19937 generator;
};

}
