// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <memory>
#include <utility>
#include <functional>
#include <mutex>

namespace squeeze::misc {

/* Generic singleton double-checked class. */
template<class T, typename Deleter = std::default_delete<T>>
class Singleton {
public:
    using Maker = std::function<std::unique_ptr<T, Deleter>()>;

    static inline void set_maker(Maker&& maker)
    {
        Singleton::maker = std::move(maker);
    }

    static T& instance()
    {
        std::call_once(once_flag, [](){ instance_ptr = maker(); });
        return *instance_ptr;
    }

private:
    inline static std::unique_ptr<T, Deleter> default_make()
    {
        return std::make_unique<T>();
    }

    inline static std::once_flag once_flag;
    inline static std::unique_ptr<T, Deleter> instance_ptr;
    inline static Maker maker = std::is_default_constructible_v<T> ? default_make : Maker{};
};

}
