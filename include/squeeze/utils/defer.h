// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <optional>
#include <type_traits>

namespace squeeze::utils {

/** Defers the function (or callable) call at scope end where this object exists.
 * Uses RAII principle to accomplish this by making the call in its destructor. */
template<typename F> requires std::is_invocable_v<F>
class Defer {
public:
    Defer(F f) : f(f)
    {
    }

    inline void cancel()
    {
        f.reset();
    }

    ~Defer()
    {
        if (f) (*f)();
    }
private:
    std::optional<F> f;
};

}
