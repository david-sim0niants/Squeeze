#pragma once

#include <mutex>
#include <optional>
#include <atomic>
#include <cstdint>

#include "squeeze/utils/defer.h"

namespace squeeze::misc {

template<typename T>
class ThreadSafeQueue {
private:
    struct Node {
        Node() noexcept = default;

        inline void emplace_value(auto&&... args)
        {
            new (value_storage) T(std::forward<decltype(args)>(args)...);
        }

        inline void destroy_value()
        {
            get_value().~T();
        }

        inline T& get_value() noexcept
        {
            return *reinterpret_cast<T *>(value_storage);
        }

        inline const T& get_value() const noexcept
        {
            return *reinterpret_cast<const T *>(value_storage);
        }

        Node *next = nullptr;
        alignas(T) std::byte value_storage[sizeof(T)];
    };

public:
    ThreadSafeQueue() noexcept : dummy_tail(new Node), head(dummy_tail)
    {
    }

    ~ThreadSafeQueue()
    {
        Node *node = head;
        while (true) {
            Node *next = node->next;
            if (node != dummy_tail.load(std::memory_order::relaxed)) {
                node->destroy_value();
                delete node;
            } else {
                delete node;
                break;
            }
            node = next;
        }
    }

    ThreadSafeQueue(ThreadSafeQueue&&) = delete;
    ThreadSafeQueue& operator=(ThreadSafeQueue&&) = delete;

    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    void push(const T& value)
    {
        emplace(value);
    }

    void push(T&& value)
    {
        emplace(std::move(value));
    }

    void emplace(auto&&... args)
    {
        Node *new_dummy_tail = new Node;

        Node *curr_tail = nullptr;
        do {
            dummy_tail.wait(nullptr, std::memory_order::acquire);
            curr_tail = dummy_tail.exchange(nullptr, std::memory_order::relaxed);
        } while (!curr_tail);

        curr_tail->next = new_dummy_tail;
        curr_tail->emplace_value(std::forward<decltype(args)>(args)...);

        dummy_tail.store(new_dummy_tail, std::memory_order::release);
        dummy_tail.notify_one();

        size.fetch_add(1, std::memory_order::release);
        size.notify_one();
    }

    void push_done() noexcept
    {
        size.fetch_or(done_flag, std::memory_order::release);
        size.notify_all();
    }

    std::optional<T> try_pop() noexcept
    {
        Node *node = nullptr;
        {
            std::scoped_lock head_lock (head_mutex);
            if ((size.load(std::memory_order::acquire) & (~done_flag)) == 0)
                return std::nullopt;
            node = pop_node_unsafe();
        }
        return unwrap_node(node);
    }

    std::optional<T> try_wait_and_pop() noexcept
    {
        Node *node = nullptr;
        {
            std::scoped_lock head_lock (head_mutex);
            size.wait(0, std::memory_order::acquire);
            if ((size.load(std::memory_order::relaxed) & (~done_flag)) == 0)
                return std::nullopt;
            node = pop_node_unsafe();
        }
        return unwrap_node(node);
    }

    inline size_t get_size() const noexcept
    {
        return size.load(std::memory_order::acquire);
    }

    static constexpr size_t get_max_size()
    {
        return max_size;
    }

private:
    Node *pop_node_unsafe()
    {
        Node *node = head;
        head = head->next;
        size.fetch_sub(1, std::memory_order::release);
        return node;
    }

    T unwrap_node(Node *node) noexcept
    {
        utils::Defer deferred_node_delete {
            [node]()
            {
                node->destroy_value();
                delete node;
            }
        };
        return std::move(node->get_value());
    }

    static constexpr size_t done_flag = (size_t(1) << (sizeof(size_t) * CHAR_WIDTH - 1));
    static constexpr size_t max_size = done_flag - 1;

    std::atomic<Node *> dummy_tail;
    Node *head;
    std::mutex head_mutex;
    std::atomic_size_t size;
};

}
