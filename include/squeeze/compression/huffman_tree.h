#pragma once

#include <cassert>
#include <deque>

#include "squeeze/error.h"
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

    template<std::input_iterator BoolInIt>
    unsigned int find_symbol(BoolInIt bool_it) const
    {
        const HuffmanTreeNode *node = this;
        while (not node->is_leaf()) {
            node = *bool_it ? node->right : node->left;
            ++bool_it;
            assert(node != nullptr && "non-full binary tree passed to the decoder");
        }

        if (node->symbol == sentinel_symbol)
            throw Exception<HuffmanTreeNode>("reached a sentinel symbol index while decoding Huffman codes");

        return node->symbol;
    }

    inline bool validate_full_tree() const
    {
        return ((left and right) and (left->validate_full_tree() and right->validate_full_tree())
             or (not left and not right));
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
    Error<HuffmanTree> build_from_codes(CodeIt code_it, CodeIt code_it_end,
            CodeLenIt code_len_it, CodeLenIt code_len_it_end)
    {
        if (code_it == code_it_end)
            return success;

        auto node_creater =
            [&nodes_storage = this->nodes_storage]() -> HuffmanTreeNode *
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

            auto e = root->insert(code, code_len, symbol, node_creater);
            if (e)
                return {"failed to build a Huffman tree", e.report()};
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
