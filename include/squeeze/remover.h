#pragma once

#include <istream>
#include <vector>
#include <queue>

#include "entry_iterator.h"
#include "error.h"

namespace squeeze {

/* Interface responsible for performing entry remove operations.
 * For optimal use of these operations will_remove method is provided
 * for registering these operations and performing them all at once
 * by calling the provided perform_removes() method. Immediate task-running remove()
 * method also exists for convenience. */
class Remover {
protected:
    struct FutureRemove;
    struct FutureRemoveCompare {
        bool operator()(const FutureRemove& a, const FutureRemove& b);
    };

public:
    explicit Remover(std::iostream& target);
    ~Remover();

    /* Register a future remove operation by providing an iterator pointing to the entry to be removed.
     * Pass an optional pointer to a future error to assign when the task is done. */
    void will_remove(const EntryIterator& it, Error<Remover> *err = nullptr);

    /* Remove an entry immediately by passing an iterator pointing to it. */
    Error<Remover> remove(const EntryIterator& it);

    /* Perform the registered removes.
     * The method guarantees that the put pointer of the target stream
     * will be at the new end of the stream */
    void perform_removes();

protected:
    std::iostream& target;
    std::priority_queue<FutureRemove, std::vector<FutureRemove>, FutureRemoveCompare> future_removes;

};

}
