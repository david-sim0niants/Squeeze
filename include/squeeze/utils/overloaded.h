// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

namespace squeeze::utils {

template<typename ...Ts>
struct Overloaded : Ts... { using Ts::operator()...; };

template<typename ...Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

}
