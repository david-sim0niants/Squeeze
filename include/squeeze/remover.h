// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <istream>
#include <vector>
#include <queue>

#include "entry_iterator.h"
#include "status.h"

namespace squeeze {

/* Interface responsible for performing entry remove operations.
 * For optimal use of these operations will_remove method is provided
 * for registering these operations and performing them all at once
 * by calling the provided perform_removes() method. Immediate task-running remove()
 * method also exists for convenience. */
class Remover {
public:
    using Stat = StatStr;

protected:
    struct FutureRemove;
    struct FutureRemoveCompare {
        bool operator()(const FutureRemove& a, const FutureRemove& b);
    };

public:
    explicit Remover(std::iostream& target);
    ~Remover();

    /* Register a future remove operation by providing an iterator pointing to the entry to be removed.
     * Pass an optional pointer to a future status to assign when the task is done. */
    void will_remove(const EntryIterator& it, Stat *err = nullptr);

    /* Remove an entry immediately by passing an iterator pointing to it. */
    Stat remove(const EntryIterator& it);

    /* Perform the registered removes.
     * The method guarantees that the put pointer of the target stream
     * will be at the new end of the stream */
    bool perform_removes();

protected:
    std::iostream& target;
    std::priority_queue<FutureRemove, std::vector<FutureRemove>, FutureRemoveCompare> future_removes;

};

}
