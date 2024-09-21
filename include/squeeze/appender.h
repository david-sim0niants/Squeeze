#pragma once

#include <ostream>
#include <vector>
#include <memory>
#include <thread>

#include "entry_input.h"
#include "status.h"
#include "append_scheduler.h"
#include "encoder.h"

namespace squeeze {

/* Interface responsible for performing appending operations.
 * For optimal use of these operations family of will_append methods are
 * provided for registering these operations and performing them all at once
 * by calling the provided perform_appends() method.
 * Immediate task-running append methods also exist for convenience.
 * All will_append/append operations require an entry input in some way to be passed.
 * Entry input is a generalized interface for providing an entry header and
 * corresponding entry content to be processed. */
class Appender {
public:
    /* Status return type of this class methods. */
    using Stat = AppendScheduler::Stat;

protected:
    struct FutureAppend {
        FutureAppend(EntryInput& entry_input, Stat *status)
            : entry_input(entry_input), status(status)
        {
        }

        EntryInput& entry_input;
        Stat *status;
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
     * Pass a reference to a future status to assign when the task is done. */
    template<typename T, typename ...Args>
    inline void will_append(StatStr& stat, Args&&... args)
        requires std::is_base_of_v<EntryInput, T>
    {
        will_append(std::make_unique<T>(std::forward<Args>(args)...), &stat);
    }

    /* Register a future append operation by passing an rvalue unique pointer to the entry input.
     * The entry input memory will be managed by the interface.
     * Pass an optional pointer to a future status to assign when the task is done. */
    void will_append(std::unique_ptr<EntryInput>&& entry_input, Stat *status = nullptr);
    /* Register a future append operation by passing a reference to the entry input.
     * The entry input memory will NOT be managed by the interface.
     * Pass an optional pointer to a future status to assign when the task is done. */
    void will_append(EntryInput& entry_input, Stat *status = nullptr);

    /* Perform the registered appends.
     * The method guarantees that the put pointer of the target stream
     * will be at the new end of the stream */
    bool perform_appends();

    /* Append an entry immediately by creating an entry input with the supplied type and parameters. */
    template<typename T, typename ...Args>
    inline Stat append(Args&&... args) requires std::is_base_of_v<EntryInput, T>
    {
        T entry_input(std::forward<Args>(args)...);
        return append(entry_input);
    }

    /* Append an entry immediately by passing a reference to the entry input. */
    Stat append(EntryInput& entry_input);

protected:
    /* Runs the scheduler tasks */
    inline bool perform_scheduled_appends()
    {
        return scheduler.run(target);
    }

    /* Schedules all registered appends. */
    bool schedule_appends();
    /* Schedules single registered append. */
    bool schedule_append(FutureAppend& future_append);
    /* Schedules a registered stream append. */
    bool schedule_append_stream(const CompressionParams& compression, std::istream& stream);
    /* Schedules a registered string append. */
    bool schedule_append_string(const CompressionParams& compression, const std::string& str);
    /* Schedules a registered stream's buffer appends. */
    bool schedule_buffer_appends(std::istream& stream);
    /* Schedules a registered stream's future buffer appends. */
    bool schedule_future_buffer_appends(const CompressionParams& compression, std::istream& stream);

    inline EncoderPool& get_encoder_pool()
    {
        if (not encoder_pool.has_value()) [[unlikely]]
            encoder_pool.emplace();
        return *encoder_pool;
    }

    std::ostream& target;
    std::vector<std::unique_ptr<EntryInput>> owned_entry_inputs;
    std::vector<FutureAppend> future_appends;
    AppendScheduler scheduler;
    std::optional<EncoderPool> encoder_pool;
};

}
