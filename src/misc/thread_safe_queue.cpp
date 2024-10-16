// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "squeeze/misc/thread_safe_queue.h"

namespace squeeze::misc::detail {

void BaseThreadSafeQueue::push_node(Node *node) noexcept
{
    std::scoped_lock tail_lock(tail_mutex);
    if (empty())
        tail = head = node;
    else
        std::exchange(tail, node)->next = node;
    size.fetch_add(1, std::memory_order::release);
    size.notify_one();
}

BaseThreadSafeQueue::Node * BaseThreadSafeQueue::try_pop_node() noexcept
{
    if (get_size_explicit(std::memory_order::relaxed) == 0)
        return nullptr;
    std::scoped_lock head_lock(head_mutex);
    return try_pop_node_unsafe();
}

BaseThreadSafeQueue::Node * BaseThreadSafeQueue::try_wait_and_pop_node() noexcept
{
    std::scoped_lock head_lock(head_mutex);
    size.wait(0, std::memory_order::acquire);
    return try_pop_node_unsafe();
}

BaseThreadSafeQueue::Node * BaseThreadSafeQueue::try_pop_node_unsafe() noexcept
{
    if (empty())
        return nullptr;

    Node *node = head;
    if (get_size_explicit(std::memory_order::acquire) == 1) {
        std::scoped_lock tail_lock(tail_mutex);
        if (head == tail) {
            tail = head = nullptr;
            size.fetch_sub(1, std::memory_order::release);
            return node;
        }
    }

    head = head->next;
    size.fetch_sub(1, std::memory_order::release);
    return node;
}

}
