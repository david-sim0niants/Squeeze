#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <type_traits>
#include <string>

#include "reader_iterator.h"
#include "entry_input.h"

namespace squeeze {

/* Interface responsible for performing entry append and remove operations.
 * For optimal use of these operations will_append/will_remove methods are
 * provided for registering these operations and performing them all at once
 * by calling the provided write() method. Immediate task-running append/remove
 * methods also exist for convenience.
 * All will_append/will_remove/append/remove operations require an entry input
 * in some way to be passed. Entry input is a generalized interface for providing
 * an entry header and corresponding entry content to be processed. */
class Writer {
protected:
    struct FutureAppend;
    struct FutureRemove;
    struct FutureRemoveCompare {
        bool operator()(const FutureRemove& a, const FutureRemove& b);
    };

public:
    /* Construct the interface by passing a reference to the iostream target to operate on. */
    explicit Writer(std::iostream& target);
    ~Writer();

    /* Register a future append operation by creating an entry input with the supplied type and parameters.
     * The entry input memory will be managed by the interface. */
    template<typename T, typename ...Args>
    inline void will_append(Args&&... args) requires std::is_base_of_v<EntryInput, T>
    {
        will_append(std::make_unique<T>(std::forward<Args>(args)...));
    }

    /* Register a future append operation by creating an entry input with the supplied type and parameters.
     * The entry input memory will be managed by the interface.
     * Pass a reference to a future error to assign when the task is done. */
    template<typename T, typename ...Args>
    inline void will_append(Error<Writer>& err, Args&&... args)
        requires std::is_base_of_v<EntryInput, T>
    {
        will_append(std::make_unique<T>(std::forward<Args>(args)...), &err);
    }

    /* Register a future append operation by passing an rvalue unique pointer to the entry input.
     * The entry input memory will be managed by the interface.
     * Pass an optional pointer to a future error to assign when the task is done. */
    void will_append(std::unique_ptr<EntryInput>&& entry_input, Error<Writer> *err = nullptr);
    /* Register a future append operation by passing a reference to the entry input.
     * The entry input memory will NOT be managed by the interface.
     * Pass an optional pointer to a future error to assign when the task is done. */
    void will_append(EntryInput& entry_input, Error<Writer> *err = nullptr);

    /* Register a future remove operation by providing an iterator pointing to the entry to be removed.
     * Pass an optional pointer to a future error to assign when the task is done. */
    void will_remove(const ReaderIterator& it, Error<Writer> *err = nullptr);

    /* Perform all the registered entry append and remove operations.
     * The method guarantees that the put pointer of the target stream will be at the new end of the stream,
     * so that the in case the stream is a file, it can be truncated to that position. */
    void write();

    /* Append an entry immediately by creating an entry input with the supplied type and parameters. */
    template<typename T, typename ...Args>
    inline Error<Writer> append(Args&&... args) requires std::is_base_of_v<EntryInput, T>
    {
        T entry_input(std::forward<Args>(args)...);
        return append(entry_input);
    }

    /* Append an entry immediately by passing a reference to the entry input. */
    Error<Writer> append(EntryInput& entry_input);
    /* Remove an entry immediately by passing an iterator pointing to it. */
    Error<Writer> remove(const ReaderIterator&& it);

protected:
    void perform_removes();
    void perform_appends();
    Error<Writer> perform_append(EntryInput& entry_input);
    Error<Writer> perform_append_stream(EntryHeader& entry_header, std::istream& input);
    Error<Writer> perform_append_string(EntryHeader& entry_header, const std::string& str);

    std::iostream& target;
    std::vector<std::unique_ptr<EntryInput>> owned_entry_inputs;
    std::vector<FutureAppend> future_appends;
    std::priority_queue<FutureRemove, std::vector<FutureRemove>, FutureRemoveCompare> future_removes;
};

}
