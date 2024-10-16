// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <type_traits>
#include <functional>

#include "squeeze/status.h"
#include "thread_safe_queue.h"

namespace squeeze::misc {

enum class TaskRunPolicy {
    NoWait,
    Wait
};

/** Simple task scheduler. Supports scheduling tasks of type Task and running them.
 * Users are responsible for calling the run (or run_till_error) methods on their own
 * and can choose to run it in parallel or in any other way. */
template<typename Task>
class TaskScheduler {
public:
    TaskScheduler() noexcept = default;

    ~TaskScheduler()
    {
        close();
    }

    TaskScheduler(TaskScheduler&&) = delete;
    TaskScheduler& operator=(TaskScheduler&&) = delete;

    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    inline void open() noexcept
    {
        task_q.open();
    }

    /** Schedule a task (uses move semantics). */
    inline void schedule(Task&& task)
    {
        task_q.push(std::move(task));
    }

    /** Emplace a task to schedule. */
    inline void schedule(auto&&... args)
    {
        task_q.emplace(std::forward<decltype(args)>(args)...);
    }

    /** Close scheduling. When all the tasks scheduled prior are done, the run(_till_error)
     * method will return. All subsequent attempts to schedule tasks will be ignored. */
    inline void close() noexcept
    {
        task_q.close();
    }

    /** Get the number of tasks left to run. */
    inline std::size_t get_nr_tasks_left() const
    {
        return task_q.get_size();
    }

    /** Run until hitting an error. This method is valid to use if calling a Task object returns a status.
     * This method is mainly supposed to be called within a different thread parallel to scheduling threads. */
    template<TaskRunPolicy policy = TaskRunPolicy::Wait, typename... Args>
    auto run_till_error(Args&&... args)
    {
        using Stat = std::invoke_result_t<Task, Args&&...>;
        while (auto task = get_task<policy>()) {
            Stat s = (*task)(std::forward<decltype(args)>(args)...);
            if (s.failed())
                return Stat(std::move(s));
        }
        return Stat(success);
    }

    /** Plain task runner.
     * Mainly supposed to be called within a different thread parallel to scheduling threads. */
    template<TaskRunPolicy policy = TaskRunPolicy::Wait>
    void run(auto&&... args)
    {
        while (auto task = get_task<policy>())
            (*task)(std::forward<decltype(args)>(args)...);
    }

private:
    template<TaskRunPolicy policy>
    inline std::optional<Task> get_task()
    {
        if constexpr (policy == TaskRunPolicy::Wait)
            return task_q.try_wait_and_pop();
        else
            return task_q.try_pop();
    }

    ThreadSafeQueue<Task> task_q;
};

}
