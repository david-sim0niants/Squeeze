#pragma once

#include <variant>

template<typename Symbol, Symbol default_symbol = Symbol()>
class HuffmanTree {
    struct Node {
        Symbol symbol;
    };
}
