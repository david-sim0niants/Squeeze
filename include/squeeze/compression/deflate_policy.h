// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include "huffman_policy.h"

namespace squeeze::compression {

template<typename T>
concept DeflatePolicy = requires
    {
        typename T::HuffmanPolicy;
        requires HuffmanPolicy<typename T::HuffmanPolicy>;
        requires T::HuffmanPolicy::code_len_limit == 15;

        typename T::CodeLenHuffmanPolicy;
        requires HuffmanPolicy<typename T::CodeLenHuffmanPolicy>;
        requires T::CodeLenHuffmanPolicy::code_len_limit == 7;
    };

struct BasicDeflatePolicy {
    using HuffmanPolicy = BasicHuffmanPolicy<15>;
    using CodeLenHuffmanPolicy = BasicHuffmanPolicy<7>;
};

}
