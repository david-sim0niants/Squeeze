// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <ostream>
#include <future>

#include "common.h"
#include "entry_header.h"
#include "misc/task_scheduler.h"

namespace squeeze {

class EntryAppendScheduler;

/** This class is responsible for scheduling append operations and
 * providing run() method to execute the scheduled tasks.
 * Each append task is an entry append task which is itself a scheduler of type
 * EntryAppendScheduler, consisting of smaller tasks like buffer/string append
 * operations, scheduling of which is supported within this class. */
class AppendScheduler {
public:
    /** Status return type of this class methods. */
    using Stat = StatStr;
    /** Future buffer type */
    using FutureBuffer = std::future<std::pair<Buffer, Stat>>;

    /** EntryAppendScheduler wrapped as a task in a callable format. */
    struct Task {
        Task(EntryHeader entry_header, Stat *stat) noexcept;
        ~Task();

        Task(Task&&) = default;
        Task& operator=(Task&&) = default;

        void operator()(std::ostream& target, bool& succeeded);

        std::unique_ptr<EntryAppendScheduler> scheduler;
    };

    AppendScheduler() noexcept;
    ~AppendScheduler();

    /** Schedule (more like start scheduling) appending a new entry and finalize the previous append.
     * The runner is responsible for encoding the entry header and passing a status via
     * the optional status pointer if needed. */
    void schedule_entry_append(EntryHeader&& entry_header, Stat *error = nullptr);
    /** Schedule error raise operation. This is necessary if an error occurs during
     * the scheduling after it has already been partially done.
     * NOTE: calling the method after finalize_entry_append() will result in a null pointer access
     * and therefore is undefined. It must be called only after schedule_entry_append(). */
    void schedule_error_raise(Stat&& error);
    /** Schedule future buffer append operation. The runner will wait for the future
     * to be satisfied and append it to the target
     * NOTE: calling the method after finalize_entry_append() will result in null pointer access
     * and therefore is undefined. It must be called only after schedule_entry_append(). */
    void schedule_buffer_append(FutureBuffer&& future_buffer);
    /** Schedule buffer append operation. The runner will just append it to the target.
     * NOTE: calling the method after finalize_entry_append() will result in null pointer access
     * and therefore is undefined. It must be called only after schedule_entry_append(). */
    void schedule_buffer_append(Buffer&& buffer);
    /** Schedule string append operation. The runner will just append it to the target.
     * NOTE: calling the method after finalize_entry_append() will result in null pointer access
     * and therefore is undefined. It must be called only after schedule_entry_append(). */
    void schedule_string_append(std::string&& str);
    /** Finalize the current entry append task. No more subsequent scheduling can be done on it.
     * The runner of the entry append task will return after completing all the pre-scheduled tasks. */
    void finalize_entry_append() noexcept;

    /** Run the scheduled tasks on the target output stream.
     * Supposed to be called asynchronously while scheduling being done synchronously. */
    inline bool run(std::ostream& target)
    {
        bool succeeded = true;
        scheduler.run(target, succeeded);
        scheduler.open();
        return succeeded;
    }

    /** Finalize the scheduler.
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
    /** Pointer to the scheduler of the last scheduled entry append task. */
    EntryAppendScheduler *last_entry_append_scheduler = nullptr;
};

class BlockAppender;

/** This class is responsible for scheduling block append operations for each individual squeeze entry. */
class EntryAppendScheduler {
public:
    /** Status return type of the methods of this class. */
    using Stat = AppendScheduler::Stat;

private:
    struct Task {
        explicit Task(std::unique_ptr<BlockAppender>&& block_appender);
        ~Task();

        Task(Task&&) = default;
        Task& operator=(Task&&) = default;

        Stat operator()(std::ostream& target);

        std::unique_ptr<BlockAppender> block_appender;
    };

public:
    EntryAppendScheduler(EntryHeader entry_header, Stat *error);
    ~EntryAppendScheduler();

    /** Schedule error raise operation. The runner will set the error pointer if (valid) and return.
     * This is necessary if an error occurs during the scheduling after it has already been partially done. */
    void schedule_error_raise(Stat&& error);
    /** Schedule future buffer append operation. The runner will wait for the future
     * to be satisfied and append it to the target. */
    void schedule_buffer_append(std::future<std::pair<Buffer, Stat>>&& future_buffer);
    /** Schedule buffer append operation. The runner will just append it to the target. */
    void schedule_buffer_append(Buffer&& buffer);
    /** Schedule string append operation. The runner will just append it to the target. */
    void schedule_string_append(std::string&& str);

    /** Finalize the scheduler. No more block append operations can be scheduled afterwards. */
    inline void finalize() noexcept
    {
        scheduler.close();
    }

    /** Run the scheduled tasks on the target output stream. */
    inline bool run(std::ostream& target)
    {
        return set_status(run_internal(target));
    }

private:
    Stat run_internal(std::ostream& target);
    bool set_status(Stat&& s);

    Stat *status;
    EntryHeader entry_header;
    misc::TaskScheduler<Task> scheduler;
};

}
