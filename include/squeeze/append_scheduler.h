#pragma once

#include <ostream>
#include <future>

#include "common.h"
#include "entry_header.h"
#include "misc/task_scheduler.h"

namespace squeeze {

class EntryAppendScheduler;

/* This class is responsible for scheduling append operations and
 * providing run() method to execute the scheduled tasks.
 * Each append task is an entry append task which is itself a scheduler of type
 * EntryAppendScheduler, consisting of smaller tasks like buffer/string append
 * operations, scheduling of which is supported within this class. */
class AppendScheduler {
public:
    /* EntryAppendScheduler wrapped as a task in a callable format. */
    struct Task {
        Task(EntryHeader entry_header, Error<> *error) noexcept;
        ~Task();

        Task(Task&&) = default;
        Task& operator=(Task&&) = default;

        void operator()(std::ostream& target);

        std::unique_ptr<EntryAppendScheduler> scheduler;
    };

    AppendScheduler() noexcept;
    ~AppendScheduler();

    /* Schedule (more like start scheduling) appending a new entry and finalize the previous append.
     * The runner is responsible for encoding the entry header and passing an error via
     * the optional error pointer if needed. */
    void schedule_entry_append(EntryHeader&& entry_header, Error<> *error = nullptr);
    /* Schedule error raise operation. This is necessary if an error occurs during
     * the scheduling after it has already been partially done.
     * NOTE: calling the method after finalize_entry_append() will result in null pointer access
     * and therefore is undefined. It must be called only after schedule_entry_append(). */
    void schedule_error_raise(Error<>&& error);
    /* Schedule future buffer append operation. The runner will wait for the future
     * to be satisfied and append it to the target
     * NOTE: calling the method after finalize_entry_append() will result in null pointer access
     * and therefore is undefined. It must be called only after schedule_entry_append(). */
    void schedule_buffer_append(std::future<std::pair<Buffer, Error<>>>&& future_buffer);
    /* Schedule buffer append operation. The runner will just append it to the target.
     * NOTE: calling the method after finalize_entry_append() will result in null pointer access
     * and therefore is undefined. It must be called only after schedule_entry_append(). */
    void schedule_buffer_append(Buffer&& buffer);
    /* Schedule string append operation. The runner will just append it to the target.
     * NOTE: calling the method after finalize_entry_append() will result in null pointer access
     * and therefore is undefined. It must be called only after schedule_entry_append(). */
    void schedule_string_append(std::string&& str);
    /* Finalize the current entry append task. No more subsequent scheduling can be done on it.
     * The runner of the entry append task will return after completing all the pre-scheduled tasks. */
    void finalize_entry_append() noexcept;

    /* Run the scheduled tasks on the target output stream.
     * Supposed to be called asynchronously while scheduling being done synchronously. */
    inline void run(std::ostream& target)
    {
        scheduler.run(target);
        scheduler.open();
    }

    /* Finalize the scheduler.
     * If the run() method was already running (perhaps in some other thread),
     * make sure it's finished before starting to schedule tasks again, otherwise
     * it may accidentally run newly scheduled tasks. */
    inline void finalize() noexcept
    {
        finalize_entry_append();
        scheduler.close();
    }

private:
    misc::TaskScheduler<Task> scheduler;
    /* Pointer to the scheduler of the last scheduled entry append task. */
    EntryAppendScheduler *last_entry_append_scheduler = nullptr;
};

class BlockAppender;

/* This class is responsible for scheduling block append operations for each individual squeeze entry. */
class EntryAppendScheduler {
private:
    struct Task {
        explicit Task(std::unique_ptr<BlockAppender>&& block_appender);
        ~Task();

        Task(Task&&) = default;
        Task& operator=(Task&&) = default;

        Error<Task> operator()(std::ostream& target);

        std::unique_ptr<BlockAppender> block_appender;
    };

public:
    EntryAppendScheduler(EntryHeader entry_header, Error<> *error);
    ~EntryAppendScheduler();

    /* Schedule error raise operation. The runner will set the error pointer if (valid) and return.
     * This is necessary if an error occurs during the scheduling after it has already been partially done. */
    void schedule_error_raise(Error<>&& error);
    /* Schedule future buffer append operation. The runner will wait for the future
     * to be satisfied and append it to the target. */
    void schedule_buffer_append(std::future<std::pair<Buffer, Error<>>>&& future_buffer);
    /* Schedule buffer append operation. The runner will just append it to the target. */
    void schedule_buffer_append(Buffer&& buffer);
    /* Schedule string append operation. The runner will just append it to the target. */
    void schedule_string_append(std::string&& str);

    /* Finalize the scheduler. No more block append operations can be scheduled afterwards. */
    inline void finalize() noexcept
    {
        scheduler.close();
    }

    /* Run the scheduled tasks on the target output stream. */
    inline void run(std::ostream& target)
    {
        set_error(run_internal(target));
    }

private:
    Error<> run_internal(std::ostream& target);
    void set_error(const Error<>& e);

    Error<> *error;
    EntryHeader entry_header;
    misc::TaskScheduler<Task> scheduler;
};

}
