#pragma once

namespace squeeze::utils {

template<typename ...Ts>
struct Overloaded : Ts... { using Ts::operator()...; };

template<typename ...Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

}