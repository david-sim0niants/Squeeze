#pragma once

#include <type_traits>
#include <functional>

#include "squeeze/error.h"
#include "thread_safe_queue.h"

namespace squeeze::misc {

/* Simple task scheduler. Supports scheduling tasks of type Task and running them.
 * Users are responsible for calling the run (or run_till_error) methods on their own
 * and can choose to run it in parallel or in any other way. */
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

    /* Schedule a task (uses move semantics). */
    inline void schedule(Task&& task)
    {
        tasks.push(std::move(task));
    }

    /* Emplace a task to schedule. */
    inline void schedule(auto&&... args)
    {
        tasks.emplace(std::forward<decltype(args)>(args)...);
    }

    /* Finalize scheduling. When all the tasks scheduled prior are done, the run(_till_error)
     * method will return. All subsequent attempts to schedule tasks will be ignored. */
    inline void finalize() noexcept
    {
        tasks.finish();
    }

    /* Get the number of tasks left to run. */
    inline size_t get_nr_tasks_left() const
    {
        return tasks.get_size();
    }

    /* Run until hitting an error. This method is valid to use if calling a Task object
     * returns an error (convertible to Error<Task>).
     * This method is mainly supposed to be called within a different thread parallel to scheduling threads. */
    Error<Task> run_till_error(auto&&... args)
        requires std::is_convertible_v<std::invoke_result_t<Task, decltype(args)...>, Error<Task>>
    {
        while (auto task = tasks.try_wait_and_pop()) {
            Error<Task> e = (*task)(std::forward<decltype(args)>(args)...);
            if (e.failed())
                return std::move(e);
        }
        return success;
    }

    /* Plain task runner. Mainly supposed to be called within a different thread parallel to scheduling threads. */
    void run(auto&&... args)
    {
        while (auto task = tasks.try_wait_and_pop())
            (*task)(std::forward<decltype(args)>(args)...);
    }

private:
    ThreadSafeQueue<Task> tasks;
};

}
