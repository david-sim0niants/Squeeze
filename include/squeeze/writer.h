#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <type_traits>
#include <string>
#include <thread>

#include "reader_iterator.h"
#include "entry_input.h"
#include "appender.h"

namespace squeeze {

/* Interface responsible for performing entry append and remove operations.
 * For optimal use of these operations will_append/will_remove methods are
 * provided for registering these operations and performing them all at once
 * by calling the provided write() method. Immediate task-running append/remove
 * methods also exist for convenience.
 * All will_append/will_remove/append/remove operations require an entry input
 * in some way to be passed. Entry input is a generalized interface for providing
 * an entry header and corresponding entry content to be processed. */
class Writer : public Appender {
protected:
    struct FutureRemove;
    struct FutureRemoveCompare {
        bool operator()(const FutureRemove& a, const FutureRemove& b);
    };

public:
    /* Construct the interface by passing a reference to the iostream target to operate on. */
    explicit Writer(std::iostream& target);
    ~Writer();

    /* Register a future remove operation by providing an iterator pointing to the entry to be removed.
     * Pass an optional pointer to a future error to assign when the task is done. */
    void will_remove(const ReaderIterator& it, Error<Writer> *err = nullptr);

    /* Perform all the registered entry append and remove operations.
     * The method guarantees that the put pointer of the target stream will be at the new end of the stream,
     * so that the in case the stream is a file, it can be truncated to that position. */
    void write(unsigned concurrency = std::thread::hardware_concurrency());

    /* Remove an entry immediately by passing an iterator pointing to it. */
    Error<Writer> remove(const ReaderIterator& it);

protected:
    void perform_removes();

    std::iostream& target;
    std::priority_queue<FutureRemove, std::vector<FutureRemove>, FutureRemoveCompare> future_removes;
};

}
