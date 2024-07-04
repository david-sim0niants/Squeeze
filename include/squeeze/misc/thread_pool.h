#pragma once

#include <thread>
#include <functional>
#include <atomic>
#include <concepts>

namespace squeeze::misc {

/* Interface for a thread pool.
 * Supports assign_task() method which blocks until a worker thread is freed.
 * Supports try_assign_task() method that does the same but returns false when all worker threads are busy.
 * Supports waiting_for_tasks() method that waits for all the assigned tasks to complete. */
class ThreadPool {
private:
    class WorkerThread;
    using Task = std::function<void ()>;

public:
    /* Initialize the thread pool and optionally provide number of worker threads to create.
     * Defaults to the number of cores in the system and it's NOT recommended to pass a bigger number. */
    explicit ThreadPool(const unsigned concurrency = std::thread::hardware_concurrency());
    ~ThreadPool();

    template<std::invocable F>
    inline void assign_task(F&& task)
    {
        return assign_task(wrap_task(std::forward<decltype(task)>(task)));
    }

    template<std::invocable F>
    inline bool try_assign_task(F&& task)
    {
        Task wraped_task {wrap_task(std::forward<decltype(task)>(task))};
        return try_assign_task(wraped_task);
    }

    void wait_for_tasks() const;

private:
    template<std::invocable F>
    Task wrap_task(F&& task) noexcept
    {
        return [&sem = sem, task = std::forward<decltype(task)>(task)]()
        {
            try {
                task();
            } catch (...) {
                sem.release();
                throw;
            }
            sem.release();
        };
    }

    void assign_task(Task&& task);
    bool try_assign_task(Task& task);
    void assign_task_unsafe(Task& task);

    std::vector<WorkerThread> worker_threads;
    std::counting_semaphore<> sem;
};

}
