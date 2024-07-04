#pragma once

#include <optional>
#include <mutex>
#include <atomic>
#include <utility>

#include "squeeze/utils/defer.h"

namespace squeeze::misc {

namespace detail {

class BaseThreadSafeQueue {
private:
    struct Node {
        Node *next = nullptr;
    };

    template<typename T>
    struct TNode : Node {
        explicit TNode(const T& value) : value(value)
        {
        }

        explicit TNode(T&& value) : value(std::move(value))
        {
        }

        explicit TNode(auto&& ...args) : value(std::forward<decltype(args)>(args)...)
        {
        }

        T value;
    };

public:
    BaseThreadSafeQueue() = default;

    BaseThreadSafeQueue(const BaseThreadSafeQueue&) = delete;
    BaseThreadSafeQueue& operator=(const BaseThreadSafeQueue&) = delete;

    BaseThreadSafeQueue(BaseThreadSafeQueue&&) = delete;
    BaseThreadSafeQueue& operator=(BaseThreadSafeQueue&&) = delete;

    template<typename T> inline bool push(T&& value)
    {
        if (finished())
            return false;
        push_node(new TNode<T>(std::forward<T>(value)));
        return true;
    }

    template<typename T, typename ...Args> inline bool emplace(Args&&... args)
    {
        if (finished())
            return false;
        push_node(new TNode<T>(std::forward<Args>(args)...));
        return true;
    }

    inline void finish() noexcept
    {
        size.fetch_or(finish_flag, std::memory_order::acq_rel);
        size.notify_all();
    }

    inline bool finished() const noexcept
    {
        return size.load(std::memory_order::acquire) & finish_flag;
    }

    template<typename T> std::optional<T> try_pop()
    {
        return unwrap_node<T>(try_pop_node());
    }

    template<typename T> std::optional<T> try_wait_and_pop()
    {
        return unwrap_node<T>(try_wait_and_pop_node());
    }

    template<typename T> void clear()
    {
        std::scoped_lock full_lock (head_mutex, tail_mutex);
        TNode<T> *node = static_cast<TNode<T> *>(head);
        while (node)
            delete std::exchange(node, static_cast<TNode<T> *>(node->next));
        size.fetch_and(~size_bits, std::memory_order::release);
        size.notify_all();
    }

    inline size_t get_size() const noexcept
    {
        return get_size_explicit(std::memory_order::acquire);
    }

    inline bool empty() const noexcept
    {
        return get_size() == 0;
    }

protected:
    ~BaseThreadSafeQueue() = default;

private:
    template<typename T> static std::optional<T> unwrap_node(Node *node)
    {
        if (!node)
            return std::nullopt;

        utils::Defer deferred_node_delete { [node](){ delete static_cast<TNode<T> *>(node); } };
        return std::move(static_cast<TNode<T> *>(node)->value);
    }

    void push_node(Node *node) noexcept;
    Node *try_pop_node() noexcept;
    Node *try_wait_and_pop_node() noexcept;
    Node *try_pop_node_unsafe() noexcept;

    inline size_t get_size_explicit(std::memory_order mo) const
    {
        return size.load(mo) & size_bits;
    }

private:
    static constexpr size_t finish_flag = (size_t)1 << (sizeof(size_t) * CHAR_WIDTH - 1);
    static constexpr size_t max_size = finish_flag - 1;
    static constexpr size_t size_bits = ~finish_flag;

    Node *head = nullptr, *tail = nullptr;
    std::mutex head_mutex, tail_mutex;
    std::atomic_size_t size = 0;
};

}

template<typename T>
class ThreadSafeQueue : private detail::BaseThreadSafeQueue {
private:
    using Base = detail::BaseThreadSafeQueue;
public:
    ~ThreadSafeQueue()
    {
        Base::clear<T>();
    }

    inline void finish() noexcept
    {
        Base::finish();
    }

    inline bool push(T&& value)
    {
        return Base::push(std::move(value));
    }

    inline bool push(const T& value)
    {
        return Base::push(value);
    }

    inline bool emplace(auto&& ...args)
    {
        return Base::emplace<T>(std::forward<decltype(args)>(args)...);
    }

    inline std::optional<T> try_pop()
    {
        return Base::try_pop<T>();
    }

    inline std::optional<T> try_wait_and_pop()
    {
        return Base::try_wait_and_pop<T>();
    }

    inline void clear()
    {
        Base::clear<T>();
    }

    inline size_t get_size() const
    {
        return Base::get_size();
    }

    inline bool empty() const
    {
        return Base::empty();
    }
};

}
