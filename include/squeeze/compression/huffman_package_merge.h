#pragma once

#include <cassert>
#include <cstddef>
#include <span>
#include <vector>
#include <array>
#include <algorithm>
#include <tuple>

#include "squeeze/utils/iterator_concepts.h"
#include "squeeze/exception.h"

namespace squeeze::compression {

using squeeze::utils::RandomAccessInputIterator;
using squeeze::utils::RandomAccessOutputIterator;

/* Package-Merge algorithm used for generating optimal length-limited Huffman codes.
 * This implementation follows the algorithm described in:
 * L.L. Larmore and D.S. Hirschberg, "A Fast Algorithm for Optimal Length-Limited Huffman Codes"
 * URL: https://ics.uci.edu/~dhirschb/pubs/LenLimHuff.pdf
 * */
template<typename Weight, typename Depth>
class HuffmanPackageMerge {
private:
    /* The width type. While the paper originally defines width of an item to be 2^(-depth),
     * and the cumulative width of nodes as sum of negative powers of 2, we avoid using
     * floating-point arithmetics by scaling width values by 2^max_depth. This results
     * in width values to be positive powers of 2 and use "levels" instead of "depths",
     * where each depth corresponds to a level with the following relation:
     *                  level = max_depth - depth */
    using Width = std::size_t;
    /* Internal implementation uses levels instead of depths that span [0, max_level]
     * range that maps to [max_depth - 1, 0] range of depths. max_depth = max_level + 1 */
    using Level = Depth;

    /* Struct representing a package. */
    struct Pack {
        Weight weight; /* Cumulative weight of the package. */
        std::size_t midct; /* Number of all nodes in the package at the current mid level. */
        Width hiwidth; /* Cumulative width of all nodes in the packages under the current mid level (having a HIgher depth)*/

        inline bool operator<(const Pack& rhs) const noexcept
        {
            return weight < rhs.weight;
        }
    };

    /* Struct representing an item, with a weight and index.
     * This is useful if items are meant to be sorted first, while still returning
     * their optimal depths (code lengths) in the original order. */
    struct Item {
        Weight weight;
        std::size_t index;

        inline bool operator<(const Item& rhs) const noexcept
        {
            return weight < rhs.weight || weight == rhs.weight && index < rhs.index;
        }
    };

    /* Context of internal methods to handle common data. */
    struct Context {
        std::span<Item> items; /* List of items. */
        std::span<std::size_t> nr_nodes_per_level; /* Number of nodes at each level. */
        std::span<Pack> pack_storage; /* A reusable storage meant to store packages. */
    };

public:
    /* Package-Merge with a compile-time known number of weights and avoids using dynamic memory.
     * Accepts an iterator pointing to depths to return depth values for the corresponding weights. */
    template<Depth max_depth, std::size_t nr_weights,
        std::input_iterator WeightIt, RandomAccessOutputIterator<Depth> DepthIt>
    static void package_merge(WeightIt weight_it, DepthIt depth_it)
    {
        std::array<Item, nr_weights> items;
        std::array<Pack, nr_weights * 2> pack_storage;

        Context ctx = {
            .items = items,
            .pack_storage = pack_storage,
        };
        make_and_filter_items(ctx.items, weight_it, depth_it);
        return package_merge_internal<max_depth>(ctx, depth_it);
    }

    /* Package-Merge with a runtime-known number of weights and uses dynamic memory.
     * Accepts an iterator pointing to depths to return depth values for the corresponding weights. */
    template<Depth max_depth, std::input_iterator WeightIt, RandomAccessOutputIterator<Depth> DepthIt>
    static void package_merge(WeightIt weight_it, std::size_t nr_weights, DepthIt depth_it)
    {
        std::vector<Item> items (nr_weights);
        std::vector<Pack> pack_storage(nr_weights * 2);

        Context ctx = {
            .items = items,
            .pack_storage = pack_storage,
        };
        make_and_filter_items(ctx.items, weight_it, depth_it);
        return package_merge_internal<max_depth>(ctx, depth_it);
    }

private:
    /* Turn weights into items, and handle and remove zero-weight items. */
    template<std::input_iterator WeightIt, RandomAccessOutputIterator<Depth> DepthIt>
    static void make_and_filter_items(std::span<Item>& items, WeightIt weight_it, DepthIt depth_it)
    {
        std::size_t nr_nz_weights = 0;
        for (std::size_t i = 0; i < items.size(); ++i, ++weight_it) {
            const Weight weight = *weight_it;
            if (0 == weight) {
                depth_it[i] = 0;
                continue;
            }

            items[nr_nz_weights].weight = weight;
            items[nr_nz_weights].index = i;
            ++nr_nz_weights;
        }
        items = items.first(nr_nz_weights);
    }

    /* Internal implementation assuming the items are ready and pre-processed. */
    template<Depth max_depth, RandomAccessOutputIterator<Depth> DepthIt>
    static void package_merge_internal(Context& ctx, DepthIt depth_it)
    {
        const std::size_t nr_items = ctx.items.size();

        if (1 == nr_items) {
            // edge case, assign 1 depth to the zero-weight item.
            depth_it[ctx.items.front().index] = 1;
            return;
        } else if (0 == nr_items) {
            return;
        }

        std::sort(ctx.items.begin(), ctx.items.end());

        std::array<std::size_t, max_depth> nr_nodes_per_level {};
        ctx.nr_nodes_per_level = nr_nodes_per_level;

        const Width target_width = Width(nr_items - 1) << max_depth;

        package_merge_impl<max_depth - 1, 0>(ctx, target_width, 0, nr_items);
        calc_depths_from_nodes_per_level<max_depth>(ctx, nr_items, depth_it);
    }

    /* Number of nodes at each level define depths for each item. This method does exactly that. */
    template<Depth max_depth, RandomAccessOutputIterator<Depth> DepthIt>
    static void calc_depths_from_nodes_per_level(Context& ctx, std::size_t nr_items, DepthIt depth_it)
    {
        static constexpr std::size_t max_level = max_depth - 1;

        std::size_t i = 0;
        for (; i < ctx.nr_nodes_per_level[0]; ++i)
            depth_it[ctx.items[i].index] = max_depth;

        for (Level level = 1; level <= max_level && i < nr_items; ++level) {
            const std::size_t nr_nodes_here = ctx.nr_nodes_per_level[level];
            const std::size_t nr_nodes_below = ctx.nr_nodes_per_level[level - 1];
            assert(nr_nodes_here >= nr_nodes_below);

            std::size_t nr_nodes_here_only = nr_nodes_here - nr_nodes_below;
            for ( ; nr_nodes_here_only > 0 && i < nr_items; --nr_nodes_here_only, ++i)
                depth_it[ctx.items[i].index] = max_depth - level;
        }
    }

    /* Recursive Package-Merge that uses O(N) space and avoids dynamic memory allocation. */
    template<Level max_level, Level level_shift>
    static void package_merge_impl(Context& ctx, Width width, std::size_t item_idx, std::size_t nr_items)
    {
        auto [midct, hiwidth] = linear_package_merge<max_level>(ctx, width, item_idx, nr_items);
        assert(hiwidth <= width);

        static constexpr Level mid_level = max_level / 2;
        assert(level_shift + mid_level < ctx.nr_nodes_per_level.size());

        ctx.nr_nodes_per_level[mid_level + level_shift] = item_idx + midct;

        if constexpr (0 != mid_level)
            package_merge_impl
                <mid_level - 1, level_shift>
                    (ctx, hiwidth, item_idx, midct);

        if constexpr (mid_level != max_level) {
            const Width midwidth = midct * ((Width(1) << (max_level + 1 - mid_level)) - 1); 
            const Width lowidth = (((width - hiwidth) >> mid_level) - midwidth) >> 1;
            assert(width == hiwidth + (lowidth << (1 + mid_level)) + (midwidth << mid_level));

            package_merge_impl
                <max_level - mid_level - 1, level_shift + mid_level + 1>
                    (ctx, lowidth, item_idx + midct, nr_items - midct);
        }
    }

    /* Package-Merge using O(N) space and calculates information only about
     * the number of nodes at the current mid_level=max_level/2 (midct) and
     * the number of nodes at levels below (hiwidth). */
    template<Level max_level>
    static std::tuple<std::size_t, Width> linear_package_merge(Context& ctx,
            Width width, const std::size_t item_idx, const std::size_t nr_items)
    {
        assert(nr_items * 2 <= ctx.pack_storage.size());
        assert(item_idx + nr_items <= ctx.items.size());

        if (0 == nr_items)
            return std::make_tuple(0, 0);

        std::size_t midct = 0; Width hiwidth = 0;

        const Item *const items = ctx.items.data() + item_idx;
        Pack *const packs = ctx.pack_storage.data();
        package_items<max_level>(items, packs, nr_items, 0);

        std::size_t nr_packs = nr_items;

        for (Level level = 0; level < max_level && width > 0 && nr_packs > 0; ++level) {
            const Width level_width = Width(1) << level;
            const bool pop_first = (width & level_width) == level_width;

            if (pop_first) {
                width -= level_width;
                midct += packs[0].midct;
                hiwidth += packs[0].hiwidth;
                --nr_packs;
            }

            package<max_level>(packs, nr_packs, pop_first);
            merge<max_level>(ctx.pack_storage.data(), nr_packs, items, nr_items, level);
        }

        
        const std::size_t nr_packs_needed = width >> max_level;
        if (width != (nr_packs_needed << max_level)) [[unlikely]]
            throw Exception<HuffmanPackageMerge>("no solution for the given width and set of weights");
        else if (nr_packs_needed > nr_packs) [[unlikely]]
            throw Exception<HuffmanPackageMerge>("insufficient amount of packages formed");

        for (std::size_t i = 0; i < nr_packs_needed; ++i) {
            midct += packs[i].midct;
            hiwidth += packs[i].hiwidth;
        }

        assert(midct <= nr_items);
        return std::make_tuple(midct, hiwidth);
    }

    /* Package packages pair-wise, forming twice less packages. */
    template<Level max_level>
    static void package(Pack *packs, std::size_t& nr_packs, bool ignore_first)
    {
        const std::size_t i_shift = ignore_first;
        for (std::size_t i = 1; i < nr_packs; i += 2)
            packs[i / 2] = combine_packs<max_level>(packs[i + i_shift - 1], packs[i + i_shift]);
        nr_packs /= 2;
        assert(std::is_sorted(packs, packs + nr_packs));
    }

    /* Make new packages from items at the current level and merge them with existing packages. */
    template<Level max_level>
    static void merge(Pack *pack_storage, std::size_t& nr_packs,
            const Item *items, std::size_t nr_items, Level level)
    {
        assert(nr_packs < nr_items);
        Pack *const packs = pack_storage;
        Pack *const level_native_packs = packs + nr_packs;

        assert(std::is_sorted(packs, packs + nr_packs));

        package_items<max_level>(items, level_native_packs, nr_items, level + 1);
        std::inplace_merge(packs, level_native_packs, level_native_packs + nr_items);
        nr_packs += nr_items;

        assert(std::is_sorted(packs, packs + nr_packs));
    }

    /* Turn items into packages. */
    template<Level max_level>
    static constexpr void package_items(const Item *items, Pack *packs, std::size_t nr_items, Level level)
    {
        for (std::size_t i = 0; i < nr_items; ++i)
            packs[i] = package_item<max_level>(items[i].weight, level);
    }

    /* Turn a single item into a package. */
    template<Level max_level>
    inline static constexpr Pack package_item(Weight weight, Level level)
    {
        return {
            .weight = weight,
            .midct = level == max_level / 2,
            .hiwidth = Width(level < max_level / 2) << level,
        };
    }

    /* Combine two packages into a single one. */
    template<Level max_level>
    inline static constexpr Pack combine_packs(const Pack& pack_0, const Pack& pack_1)
    {
        return {
            .weight = pack_0.weight + pack_1.weight,
            .midct = pack_0.midct + pack_1.midct,
            .hiwidth = pack_0.hiwidth + pack_1.hiwidth,
        };
    }
};

}
