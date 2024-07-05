#include "squeeze/misc/thread_pool.h"
#include "squeeze/exception.h"

namespace squeeze::misc {

class ThreadPool::WorkerThread {
public:
    /* Type of the state of the worker thread. */
    enum class State : uint8_t {
        Idle, /* Free to use state. */
        Starting, /* A short-lived state of being acquired but not ready to run */
        Running, /* Running state */
        Stopping, /* A short-lived state of stopping the thread. */
    };

    WorkerThread() : state(State::Idle), internal(std::mem_fn(&WorkerThread::run), this)
    {
    }

    ~WorkerThread()
    {
        wait_for_task();
        state.store(State::Stopping, std::memory_order::relaxed);
        state.notify_one();
        internal.join();
    }

    WorkerThread(const WorkerThread&) = delete;
    WorkerThread(WorkerThread&&) = delete;

    WorkerThread& operator=(const WorkerThread&) = delete;
    WorkerThread& operator=(WorkerThread&&) = delete;

    /* Try assigning a task to the scheduler. Will fail if already busy but might as well fail spuriously. */
    bool try_assign_task(Task& task)
    {
        if (!task)
            throw Exception<ThreadPool>("invalid task");

        State expected = State::Idle;
        if (!state.compare_exchange_weak(expected, State::Starting,
                    std::memory_order::acquire, std::memory_order::relaxed))
            return false;
        this->task.swap(task);
        state.store(State::Running, std::memory_order::release);
        state.notify_one();
        return true;
    }

    /* Wait for task to complete. */
    void wait_for_task() const noexcept
    {
        state.wait(State::Running, std::memory_order::acquire);
    }

private:
    void run()
    {
        while (true) {
            state.wait(State::Idle, std::memory_order::acquire);
            state.wait(State::Starting, std::memory_order::acquire);

            if (state.load(std::memory_order::relaxed) == State::Stopping)
                break;

            task();
            state.store(State::Idle, std::memory_order::release);
            state.notify_one();
        }
    }

    std::atomic<State> state;
    Task task;
    std::thread internal;
};

ThreadPool::ThreadPool(const unsigned concurrency)
    : worker_threads(concurrency), sem(concurrency)
{
}

ThreadPool::~ThreadPool() = default;

void ThreadPool::assign_task(Task&& task)
{
    sem.acquire();
    assign_task_unsafe(task);
}

bool ThreadPool::try_assign_task(Task& task)
{
    if (sem.try_acquire()) {
        assign_task_unsafe(task);
        return true;
    } else {
        return false;
    }
}

/* "Runs over" the worker threads to find a free thread to acquire. */
void ThreadPool::assign_task_unsafe(Task& task)
{
    while (true)
        for (auto& worker_thread : worker_threads)
            if (worker_thread.try_assign_task(task))
                return;
}

void ThreadPool::wait_for_tasks() const
{
    for (const auto& worker_thread : worker_threads)
        worker_thread.wait_for_task();
}

}
