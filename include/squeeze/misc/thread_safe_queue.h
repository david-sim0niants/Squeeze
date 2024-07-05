#pragma once

#include <optional>
#include <mutex>
#include <atomic>
#include <utility>

#include "squeeze/utils/defer.h"

namespace squeeze::misc {

namespace detail {

/* This class implements the core functionalities of a thread-safe queue.
 * It's separated from the template ThreadSafeQueue to be able to implement
 * type-agnostic functionalities in the source file.
 * Internally uses a linked list and two mutexes to protect the head and tail nodes,
 * as well as an atomic size variable to optimize the lock and wait/notify usages. */
class BaseThreadSafeQueue {
private:
    /* Type-agnostic abstract node type. */
    struct Node {
        Node *next = nullptr;
    };

    /* Value-type-specific node type. */
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

    /* Usage of copy/move constructors and assignment operators is discouraged and
     * otherwise wouldn't have a well defined implementation (per my guess). */

    BaseThreadSafeQueue(const BaseThreadSafeQueue&) = delete;
    BaseThreadSafeQueue& operator=(const BaseThreadSafeQueue&) = delete;

    BaseThreadSafeQueue(BaseThreadSafeQueue&&) = delete;
    BaseThreadSafeQueue& operator=(BaseThreadSafeQueue&&) = delete;

    /* Push value of type T. */
    template<typename T> inline bool push(T&& value)
    {
        if (is_closed())
            return false;
        push_node(new TNode<T>(std::forward<T>(value)));
        return true;
    }

    /* Emplace value of type T with arguments of type Args...
     * Returns true on success and false otherwise (mostly when the queue is marked as closed)*/
    template<typename T, typename ...Args> inline bool emplace(Args&&... args)
    {
        if (is_closed())
            return false;
        push_node(new TNode<T>(std::forward<Args>(args)...));
        return true;
    }

    inline void close() noexcept
    {
        size.fetch_or(close_flag, std::memory_order::acq_rel);
        size.notify_all();
    }

    inline bool is_closed() const noexcept
    {
        return size.load(std::memory_order::acquire) & close_flag;
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
    /* Extracts and returns the value from the node if it's not null and
     * then deletes the node. */
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
    static constexpr size_t close_flag = (size_t)1 << (sizeof(size_t) * CHAR_WIDTH - 1);
    static constexpr size_t max_size = close_flag - 1;
    static constexpr size_t size_bits = ~close_flag;

    Node *head = nullptr, *tail = nullptr;
    std::mutex head_mutex, tail_mutex;
    std::atomic_size_t size = 0;
};

}

/* A thread-safe queue interface. Supports pushing values, try popping them, try waiting and popping them, closing etc. */
template<typename T>
class ThreadSafeQueue : private detail::BaseThreadSafeQueue {
    using Base = detail::BaseThreadSafeQueue;
public:
    ThreadSafeQueue() = default;

    ~ThreadSafeQueue()
    {
        Base::clear<T>();
    }

    /* Close the queue. This makes threads, that are waiting for a value to appear,
     * be notified that it's over and no more values will be pushed anymore.
     * Any further attempts to push a value to the queue will fail gracefully. */
    inline void close() noexcept
    {
        Base::close();
    }

    /* Returns whether the queue is closed. */
    inline bool is_closed() const noexcept
    {
        return Base::is_closed();
    }

    /* Push value (using move semantics). */
    inline bool push(T&& value)
    {
        return Base::push(std::move(value));
    }

    /* Push value (using copy semantics). */
    inline bool push(const T& value)
    {
        return Base::push(value);
    }

    /* Emplace value using args... */
    inline bool emplace(auto&& ...args)
    {
        return Base::emplace<T>(std::forward<decltype(args)>(args)...);
    }

    /* Try to pop a value. Returns an optional which will have a value only if
     * the queue wasn't empty at the time of popping. */
    inline std::optional<T> try_pop()
    {
        return Base::try_pop<T>();
    }

    /* Try popping a value by letting the method wait for a value to appear if the queue is currently empty.
     * Returns an optional with a value only if the queue hasn't closed while it was empty. */
    inline std::optional<T> try_wait_and_pop()
    {
        return Base::try_wait_and_pop<T>();
    }

    /* Empties the queue. */
    inline void clear()
    {
        Base::clear<T>();
    }

    /* Returns number of elements in the queue. */
    inline size_t get_size() const
    {
        return Base::get_size();
    }

    /* Returns whether the queue is empty. */
    inline bool empty() const
    {
        return Base::empty();
    }
};

}
