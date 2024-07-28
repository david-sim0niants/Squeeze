#pragma once

#include <type_traits>
#include <iterator>
#include <concepts>
#include <vector>
#include <algorithm>
#include <bitset>
#include <climits>

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

template<HuffmanPolicy Policy = BasicHuffmanPolicy<16>>
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

public:
    template<RandomAccessInputIterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void sort_find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it)
    {
        package_merge<false>(freq_it, freq_it_end, code_len_it, code_len_limit);
    }

    template<RandomAccessInputIterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void sort_find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it,
            CodeLen custom_limit)
    {
        package_merge<false>(freq_it, freq_it_end, code_len_it, custom_limit);
    }

    template<std::input_iterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it)
    {
        package_merge<true>(freq_it, freq_it_end, code_len_it, code_len_limit);
    }

    template<std::input_iterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static inline void find_code_lengths(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it,
            CodeLen custom_limit)
    {
        package_merge<true>(freq_it, freq_it_end, code_len_it, custom_limit);
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
            sum_code += (unsigned long long)(1) << (code_len_limit - *code_len_it);
        }

        return sum_code == (unsigned long long)(1) << code_len_limit;
    }

    template<std::input_iterator CodeLenIt, RandomAccessOutputIterator<Code> CodeIt>
    static inline void gen_codes(CodeLenIt code_len_it, CodeLenIt code_len_end, CodeIt code_it)
    {
        if (code_len_it == code_len_end)
            return;

        auto code_lens = get_sorted_code_lens(code_len_it, code_len_end);
        Code prev_code {};
        *(code_it + code_lens.front().second) = prev_code;

        for (std::size_t i = 1; i < code_lens.size(); ++i) {
            const auto [code_len, code_idx] = code_lens[i];
            const auto [prev_code_len, _] = code_lens[i - 1];
            const Code code = (++prev_code) << (code_len - prev_code_len);
            *(code_it + code_idx) = code;
            prev_code = code;
        }
    }

private:
    template<bool assume_sorted, std::input_iterator FreqIt, RandomAccessOutputIterator<CodeLen> CodeLenIt>
    static void package_merge(FreqIt freq_it, FreqIt freq_it_end, CodeLenIt code_len_it, CodeLen depth)
    {
        if (freq_it == freq_it_end)
            return;

        const PackSet packset_per_level = package_freqs<assume_sorted>(freq_it, freq_it_end);
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

    template<bool assume_sorted, std::input_iterator FreqIt>
    static PackSet package_freqs(FreqIt freq_it, FreqIt freq_it_end)
    {
        PackSet packset;
        for (; freq_it != freq_it_end; ++freq_it)
            packset.emplace_back(
                    *freq_it, Symset{static_cast<Symset::value_type>(packset.size())});
        if constexpr (not assume_sorted)
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

    template<std::input_iterator CodeLenIt>
    static std::vector<std::pair<CodeLen, std::size_t>>
        get_sorted_code_lens(CodeLenIt code_len_it, CodeLenIt code_len_end)
    {
        std::vector<std::pair<CodeLen, std::size_t>> code_lens;
        code_lens.reserve(std::distance(code_len_it, code_len_end));
        for (; code_len_it != code_len_end; ++code_len_it)
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
