#pragma once

#include <type_traits>
#include <functional>

#include "squeeze/error.h"
#include "thread_safe_queue.h"

namespace squeeze::misc {

template<typename Task>
class TaskScheduler {
public:
    TaskScheduler() noexcept = default;

    ~TaskScheduler()
    {
        finalize();
    }

    TaskScheduler(TaskScheduler&&) = delete;
    TaskScheduler& operator=(TaskScheduler&&) = delete;

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    void schedule(Task&& task)
    {
        tasks.push(std::move(task));
    }

    void schedule(auto&&... args)
    {
        tasks.emplace(std::forward<decltype(args)>(args)...);
    }

    inline void finalize() noexcept
    {
        tasks.push_done();
    }

    /* Supposed to be called in a different thread. */
    Error<Task> run_till_error(auto&&... args)
        requires std::is_convertible_v<std::invoke_result_t<Task, decltype(args)...>, Error<Task>>
    {
        while (auto task = tasks.try_wait_and_pop()) {
            Error<Task> e = (*task)(std::forward<decltype(args)>(args)...);
            if (e.failed())
                return e;
        }
        return success;
    }

    /* Supposed to be called in a different thread. */
    void run(auto&&... args)
    {
        while (auto task = tasks.try_wait_and_pop())
            (*task)(std::forward<decltype(args)>(args)...);
    }

private:
    ThreadSafeQueue<Task> tasks;
};

}
