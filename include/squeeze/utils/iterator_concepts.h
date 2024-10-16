// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <iterator>

namespace squeeze::utils {

template<typename Iter>
concept RandomAccessInputIterator = std::random_access_iterator<Iter> and std::input_iterator<Iter>;

template<typename Iter, typename T>
concept RandomAccessOutputIterator = std::random_access_iterator<Iter> and std::output_iterator<Iter, T>;


}
