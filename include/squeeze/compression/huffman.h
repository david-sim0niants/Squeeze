#pragma once

#include <type_traits>
#include <iterator>
#include <concepts>
#include <vector>
#include <algorithm>
#include <bitset>
#include <climits>

#include "squeeze/error.h"
#include "squeeze/exception.h"

namespace squeeze::compression {

template<typename T>
concept AddableComparable = requires(T a, T b) { { a + b } -> std::same_as<T>; } and
                            requires(T a, T b) { { a < b } -> std::convertible_to<bool>; };

template<typename Iter>
concept RandomAccessInputIterator = std::random_access_iterator<Iter> and std::input_iterator<Iter>;

template<typename Iter, typename T>
concept RandomAccessOutputIterator = std::random_access_iterator<Iter> and std::output_iterator<Iter, T>;

template<typename T>
concept HuffmanCode = std::is_default_constructible_v<T>
    and std::is_copy_constructible_v<T> and std::is_copy_assignable_v<T>
    and requires(T t, int x)
        {
            { ++t } -> std::same_as<T>;
            { t << x } -> std::same_as<T>;
        };

template<typename T>
concept HuffmanPolicy = requires
    {
        typename T::Freq; typename T::CodeLen;
        requires AddableComparable<typename T::Freq>;
        requires std::integral<typename T::CodeLen>;
        { T::code_len_limit } -> std::same_as<const typename T::CodeLen&>;
        requires T::code_len_limit <= sizeof(unsigned long long) * CHAR_BIT;
    };

template<unsigned int codelen_limit> requires (codelen_limit <= sizeof(unsigned long long) * CHAR_BIT)
struct BasicHuffmanPolicy {
    using Freq = unsigned int;
    using CodeLen = unsigned int;
    static constexpr CodeLen code_len_limit = codelen_limit;
};

class HuffmanTreeNode {
public:
    HuffmanTreeNode() = default;

    explicit HuffmanTreeNode(unsigned int symbol) noexcept : symbol(0)
    {
    }

    explicit HuffmanTreeNode(HuffmanTreeNode *left, HuffmanTreeNode *right) noexcept
        : left(left), right(right)
    {
    }

    inline HuffmanTreeNode *get_left() const noexcept
    {
        return left;
    }

    inline HuffmanTreeNode *get_right() const noexcept
    {
        return right;
    }

    inline unsigned int get_symbol() const noexcept
    {
        return symbol;
    }

    inline bool is_leaf() const noexcept
    {
        return left == nullptr and right == nullptr;
    }

    template<typename Code, typename CodeLen, typename CreateNode>
    Error<HuffmanTreeNode> insert(Code code, CodeLen code_len,
            unsigned int symbol, CreateNode create_node)
    {
        HuffmanTreeNode *node = this;
        while (code_len--) {
            if (code[code_len]) {
                if (nullptr == node->right) {
                    node->right = create_node();
                    if (nullptr == node->right)
                        return "can't create a node anymore";
                }
                node = node->right;
            } else {
                if (nullptr == node->left) {
                    node->left = create_node();
                    if (nullptr == node->left)
                        return "can't create a node anymore";
                }
                node = node->left;
            }
        }

        if (not node->is_leaf())
            return "attempt to insert a code that is a prefix of another code";

        node->symbol = symbol;
        return success;
    }

    inline bool validate_full_tree() const
    {
        return ((left and right) and (left->validate_full_tree() and right->validate_full_tree())
             or (not left and not right));
    }

private:
    HuffmanTreeNode *left = nullptr, *right = nullptr;
    unsigned int symbol = 0;
};

class HuffmanTree {
public:
    inline const HuffmanTreeNode *get_root() const noexcept
    {
        return root;
    }

    template<std::input_iterator CodeIt, std::input_iterator CodeLenIt>
    Error<HuffmanTree> build_from_codes(CodeIt code_it, CodeIt code_it_end,
            CodeLenIt code_len_it, CodeLenIt code_len_it_end)
    {
        if (code_it == code_it_end)
            return success;

        std::size_t nr_codes = std::distance(code_it, code_it_end);
        nodes_storage.reserve(2 * nr_codes - 1);
        nodes_storage.emplace_back();
        root = &nodes_storage.back();

        unsigned int symbol = 0;
        for (; code_it != code_it_end && code_len_it != code_len_it_end;
                ++code_it, ++code_len_it) {
            auto e = root->insert(*code_it, *code_len_it, symbol,
                    [&nodes_storage = this->nodes_storage]() -> HuffmanTreeNode *
                    {
                        if (nodes_storage.size() == nodes_storage.capacity())
                            return nullptr;
                        nodes_storage.emplace_back();
                        return &nodes_storage.back();
                    });
            if (e)
                return {"failed to build Huffman tree", e.report()};
            ++symbol;
        }

        return success;
    }

private:
    std::vector<HuffmanTreeNode> nodes_storage;
    HuffmanTreeNode *root = nullptr;
};

template<HuffmanPolicy Policy = BasicHuffmanPolicy<15>>
class Huffman {
public:
    using Freq = typename Policy::Freq;
    using CodeLen = typename Policy::CodeLen;
    static constexpr CodeLen code_len_limit = Policy::code_len_limit;
    using Code = std::bitset<code_len_limit>;

private:
    using Symset = std::vector<unsigned int>;

    struct Pack {
        Freq freq;
        Symset symset;

        explicit Pack(Freq freq, Symset symset = {}) : freq(freq), symset(symset)
        {
        }

        explicit Pack(Pack left, Pack right) : freq(left.freq + right.freq), symset()
        {
            symset.reserve(left.symset.size() + right.symset.size());
            symset.insert(symset.end(), left.symset.begin(), left.symset.end());
            symset.insert(symset.end(), right.symset.begin(), right.symset.end());
        }
    };

    using PackSet = std::vector<Pack>;

    enum SortAssume {
        AssumeSorted, DontAssumeSorted
    };

public:
    template<RandomAccessInputIterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void sort_find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it)
    {
        package_merge<DontAssumeSorted>(freq_it, freq_it_end, code_len_it, code_len_limit);
    }

    template<RandomAccessInputIterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void sort_find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it,
            CodeLen custom_limit)
    {
        package_merge<DontAssumeSorted>(freq_it, freq_it_end, code_len_it, custom_limit);
    }

    template<std::input_iterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it)
    {
        package_merge<AssumeSorted>(freq_it, freq_it_end, code_len_it, code_len_limit);
    }

    template<std::input_iterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it,
            CodeLen custom_limit)
    {
        package_merge<AssumeSorted>(freq_it, freq_it_end, code_len_it, custom_limit);
    }

    template<std::input_iterator CodeLenIt>
    static bool validate_code_lens(CodeLenIt code_len_it, CodeLenIt code_len_it_end)
    {
        if (code_len_it == code_len_it_end)
            return true;

        unsigned long long sum_code = 0;
        for (; code_len_it != code_len_it_end; ++code_len_it) {
            CodeLen code_len = *code_len_it;
            if (code_len == 0 or code_len > code_len_limit)
                return false;
            sum_code += 1ULL << (code_len_limit - *code_len_it);
        }

        return sum_code == (1ULL << code_len_limit);
    }

    template<std::input_iterator CodeLenIt, RandomAccessOutputIterator<Code> CodeIt>
    static inline void gen_codes(CodeLenIt code_len_it, CodeLenIt code_len_it_end, CodeIt code_it)
    {
        if (code_len_it == code_len_it_end)
            return;

        auto code_lens = get_sorted_code_lens(code_len_it, code_len_it_end);
        Code prev_code {};
        *(code_it + code_lens.front().second) = prev_code;

        for (std::size_t i = 1; i < code_lens.size(); ++i) {
            const auto [code_len, code_idx] = code_lens[i];
            const auto [prev_code_len, _] = code_lens[i - 1];
            const Code code = Code(prev_code.to_ullong() + 1) << (code_len - prev_code_len);
            *(code_it + code_idx) = code;
            prev_code = code;
        }
    }

private:
    template<SortAssume sort_assume,
        std::input_iterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static void package_merge(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it, CodeLen depth)
    {
        if (freq_it == freq_it_end)
            return;

        const PackSet packset_per_level = package_freqs<sort_assume>(freq_it, freq_it_end);
        if (packset_per_level.size() > (1 << depth))
            throw Exception<Huffman>("too many symbols or too small depth");
        if (depth == 0) {
            *code_len_it = 1;
            return;
        }

        PackSet packset;
        while (depth--) {
            package(packset);
            merge(packset, packset_per_level);
        }
        calc_length(packset, packset_per_level.size(), code_len_it);
    }

    template<SortAssume sort_assume, std::input_iterator FreqIt>
    static PackSet package_freqs(FreqIt freq_it, FreqIt freq_it_end)
    {
        PackSet packset;
        for (; freq_it != freq_it_end; ++freq_it)
            packset.emplace_back(
                    *freq_it, Symset{static_cast<Symset::value_type>(packset.size())});
        if constexpr (sort_assume == DontAssumeSorted)
            std::sort(packset.begin(), packset.end(), compare_packs);
        return packset;
    }

    static void package(PackSet& packset)
    {
        for (size_t i = 1; i < packset.size(); i += 2)
            packset[i / 2] = Pack(packset[i - 1], packset[i]);
        packset.erase(packset.begin() + (packset.size() / 2), packset.end());
    }

    static void merge(PackSet& packset, const PackSet& packset_per_level)
    {
        packset.reserve(packset.size() + packset_per_level.size());
        std::copy(packset_per_level.begin(), packset_per_level.end(), std::back_inserter(packset));
        std::inplace_merge(packset.begin(), packset.end() - packset_per_level.size(), packset.end(),
                compare_packs);
    }

    template<RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static void calc_length(const PackSet& packset, std::size_t nr_syms, CodeLenIt code_len_it)
    {
        std::fill_n(code_len_it, nr_syms, 0);
        std::size_t nr_packs = 2 * nr_syms - 2;
        for (std::size_t i = 0; i < nr_packs; ++i) {
            for (auto sym : packset[i].symset) {
                ++*(code_len_it + sym);
            }
        }
    }

    template<RandomAccessInputIterator CodeLenIt>
    static std::vector<std::pair<CodeLen, std::size_t>>
        get_sorted_code_lens(CodeLenIt code_len_it, CodeLenIt code_len_it_end)
    {
        std::vector<std::pair<CodeLen, std::size_t>> code_lens;
        code_lens.reserve(std::distance(code_len_it, code_len_it_end));
        for (; code_len_it != code_len_it_end; ++code_len_it)
            code_lens.emplace_back(*code_len_it, code_lens.size());
        std::sort(code_lens.begin(), code_lens.end());
        return code_lens;
    }

    inline static bool compare_packs(const Pack& left, const Pack& right)
    {
        return left.freq < right.freq;
    }
};

}
