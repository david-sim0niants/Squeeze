#pragma once

#include <ostream>
#include <future>

#include "common.h"
#include "entry_header.h"
#include "squeeze/misc/task_scheduler.h"

namespace squeeze{

class BlockAppender;

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

    void schedule_error_raise(Error<>&& error);
    void schedule_buffer_append(std::future<Buffer>&& future_buffer);
    void schedule_buffer_append(Buffer&& buffer);
    void schedule_string_append(std::string&& str);

    inline void run(std::ostream& target)
    {
        set_error(run_internal(target));
    }

    inline void finalize() noexcept
    {
        scheduler.finalize();
    }

private:
    Error<> run_internal(std::ostream& target);
    void set_error(const Error<>& e);

    Error<> *error;
    EntryHeader entry_header;
    misc::TaskScheduler<Task> scheduler;
};

class AppendScheduler {
public:
    struct Task {
        Task(EntryHeader entry_header, Error<> *error) noexcept;
        ~Task();

        Task(Task&&) = default;
        Task& operator=(Task&&) = default;

        inline void operator()(std::ostream& target)
        {
            scheduler->run(target);
        }

        std::unique_ptr<EntryAppendScheduler> scheduler;
    };

    AppendScheduler() noexcept;
    ~AppendScheduler();

    void schedule_entry_append(EntryHeader&& entry_header, Error<> *error = nullptr);
    void schedule_error_raise(Error<>&& error);
    void schedule_buffer_append(std::future<Buffer>&& future_buffer);
    void schedule_buffer_append(Buffer&& buffer);
    void schedule_string_append(std::string&& str);
    void finalize_entry_append() noexcept;

    inline void run(std::ostream& target)
    {
        scheduler.run(target);
    }

    inline void finalize() noexcept
    {
        finalize_entry_append();
        scheduler.finalize();
    }

private:
    misc::TaskScheduler<Task> scheduler;
    EntryAppendScheduler *last_entry_append_scheduler = nullptr;
};

}
