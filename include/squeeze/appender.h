#pragma once

#include <ostream>
#include <vector>
#include <memory>
#include <thread>

#include "append_scheduler.h"
#include "entry_input.h"
#include "encode.h"
#include "error.h"

namespace squeeze {

/* Interface responsible for performing appending operations.
 * For optimal use of these operations family of will_append methods are
 * provided for registering these operations and performing them all at once
 * by calling the provided perform_appends() method. Immediate task-running append
 * methods also exist for convenience.
 * All will_append/append operations require an entry input in some way to be passed.
 * Entry input is a generalized interface for providing an entry header and
 * corresponding entry content to be processed. */
class Appender {
protected:
    struct FutureAppend {
        FutureAppend(EntryInput& entry_input, Error<Appender> *error)
            : entry_input(entry_input), error(error)
        {
        }

        EntryInput& entry_input;
        Error<Appender> *error;
    };

    struct Context {
        misc::ThreadPool thread_pool;
        EncoderPool encoder_pool;
        AppendScheduler scheduler;

        explicit Context(unsigned concurrency);
    };

public:
    explicit Appender(std::ostream& target);
    ~Appender();

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
    inline void will_append(Error<Appender>& err, Args&&... args)
        requires std::is_base_of_v<EntryInput, T>
    {
        will_append(std::make_unique<T>(std::forward<Args>(args)...), &err);
    }

    /* Register a future append operation by passing an rvalue unique pointer to the entry input.
     * The entry input memory will be managed by the interface.
     * Pass an optional pointer to a future error to assign when the task is done. */
    void will_append(std::unique_ptr<EntryInput>&& entry_input, Error<Appender> *err = nullptr);
    /* Register a future append operation by passing a reference to the entry input.
     * The entry input memory will NOT be managed by the interface.
     * Pass an optional pointer to a future error to assign when the task is done. */
    void will_append(EntryInput& entry_input, Error<Appender> *err = nullptr);

    void perform_appends(unsigned concurrency = std::thread::hardware_concurrency());

    /* Append an entry immediately by creating an entry input with the supplied type and parameters. */
    template<typename T, typename ...Args>
    inline Error<Appender> append(Args&&... args) requires std::is_base_of_v<EntryInput, T>
    {
        T entry_input(std::forward<Args>(args)...);
        return append(entry_input);
    }

    /* Append an entry immediately by passing a reference to the entry input. */
    Error<Appender> append(EntryInput& entry_input);

protected:
    void schedule_appends(Context& context);
    Error<Appender> schedule_append(Context& context, FutureAppend& future_append);
    static Error<Appender> schedule_append_stream(
            Context& context, const CompressionParams& compression, std::istream& stream);
    static Error<Appender> schedule_append_string(
            Context& context, const CompressionParams& compression, const std::string& str);

    std::ostream& target;
    std::vector<std::unique_ptr<EntryInput>> owned_entry_inputs;
    std::vector<FutureAppend> future_appends;
};

}
