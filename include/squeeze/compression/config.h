#pragma once

#include <array>
#include <utility>

#include "params.h"
#include "squeeze/exception.h"

namespace squeeze::compression {

constexpr std::size_t sentinel_block_size = std::size_t(-1);

constexpr std::array<std::size_t, 9> huffman_block_sizes_per_level = {
        4 << 10,
        4 << 10,
        8 << 10,
        16 << 10,
        24 << 10,
        32 << 10,
        48 << 10,
        64 << 10,
        128 << 10,
    };

constexpr std::array<uint8_t, 2> min_level_per_method = {0, 1};
constexpr std::array<uint8_t, 2> max_level_per_method = {0, huffman_block_sizes_per_level.size() - 1};

constexpr std::pair<uint8_t, uint8_t> get_min_max_levels(CompressionMethod method)
{
    switch (method) {
        using enum CompressionMethod;
    case None:
        return std::make_pair(min_level_per_method[0], max_level_per_method[0]);
    case Huffman:
        return std::make_pair(min_level_per_method[1], max_level_per_method[1]);
    default:
        throw BaseException("invalid compression method or an unimplemented one");
    }
}

constexpr std::size_t get_block_size(CompressionParams params)
{
    auto [min_level, max_level] = get_min_max_levels(params.method);
    if (params.level < min_level || max_level < params.level)
        throw BaseException("compression level is out of range");

    switch (params.method) {
        using enum CompressionMethod;
    case Huffman:
        return huffman_block_sizes_per_level[params.level];
    case None:
    default:
        throw BaseException("invalid compression method or an unimplemented one");
    }
}

static constexpr std::size_t lz77_nr_levels = 9;
constexpr std::array<std::size_t, lz77_nr_levels> lz77_lazy_match_threshold_per_level = {
    0, 32, 64, 96, 128, 160, 192, 224, 256
};

constexpr std::array<std::size_t, lz77_nr_levels> lz77_match_insert_threshold_per_level {
    1, 5, 6, 7, 8, 9, 10, 11, 12
};

}
