// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <cassert>
#include <deque>

#include "squeeze/status.h"
#include "squeeze/exception.h"

namespace squeeze::compression {

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
    StatStr insert(Code code, CodeLen code_len, unsigned int symbol, CreateNode create_node)
    {
        HuffmanTreeNode *node = this;
        while (code_len--) {
            if (code[code_len]) {
                if (nullptr == node->right) {
                    node->right = create_node();
                    if (nullptr == node->right)
                        return "can't create a node anymore";
                } else if (node->right->is_leaf()) {
                    return "attempt to insert a code that is a prefix of another code";
                }
                node = node->right;
            } else {
                if (nullptr == node->left) {
                    node->left = create_node();
                    if (nullptr == node->left)
                        return "can't create a node anymore";
                } else if (node->left->is_leaf()) {
                    return "attempt to insert a code that is a prefix of another code";
                }
                node = node->left;
            }

        }
        node->symbol = symbol;
        return success;
    }

    inline std::optional<unsigned int> decode_sym_from(auto& bit_decoder) const
    {
        const HuffmanTreeNode *node = this;
        bool bit;
        while (!node->is_leaf() && bit_decoder.read_bit(bit))
            node = bit ? node->get_right() : node->get_left();

        if (node->is_leaf() && !node->is_sentinel())
            return node->get_symbol();
        else
            return std::nullopt;
    }

    inline bool validate_full_tree() const noexcept
    {
        return ((left and right) and (left->validate_full_tree() and right->validate_full_tree())
             or (not left and not right));
    }

    inline bool is_sentinel() const noexcept
    {
        return symbol == sentinel_symbol;
    }

    static constexpr unsigned int sentinel_symbol = (unsigned int)(-1);

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
    StatStr build_from_codes(CodeIt code_it, CodeIt code_it_end,
            CodeLenIt code_len_it, CodeLenIt code_len_it_end)
    {
        if (code_it == code_it_end)
            return success;

        auto node_creater = [&nodes_storage = this->nodes_storage]() -> HuffmanTreeNode *
            {
                nodes_storage.emplace_back();
                return &nodes_storage.back();
            };

        nodes_storage.emplace_back();
        root = &nodes_storage.back();

        for (unsigned int symbol = 0; code_it != code_it_end && code_len_it != code_len_it_end;
                ++code_it, ++code_len_it, ++symbol) {
            auto code = *code_it;
            auto code_len = *code_len_it;
            if (code_len == 0)
                continue;

            StatStr s = root->insert(code, code_len, symbol, node_creater);
            if (s.failed())
                return {"failed inserting a code into a Huffman tree", s};
        }

        if (root->is_leaf()) {
            root = nullptr;
            nodes_storage.clear();
        } else if (root->get_left()->is_leaf() and root->get_right() == nullptr) {
            root->insert(std::iter_value_t<CodeIt>(1), 1, HuffmanTreeNode::sentinel_symbol, node_creater);
        }

        return success;
    }

private:
    std::deque<HuffmanTreeNode> nodes_storage;
    HuffmanTreeNode *root = nullptr;
};

}
