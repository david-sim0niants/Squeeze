#include "squeeze/misc/thread_pool.h"
#include "squeeze/exception.h"

#include "squeeze/logging.h"

namespace squeeze::misc {

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::misc::ThreadPool::"

class ThreadPool::WorkerThread {
public:
    enum class State : uint8_t {
        Idle,
        Starting,
        Running,
        Stopping,
    };

    WorkerThread() : state(State::Idle), internal(std::mem_fn(&WorkerThread::run), this)
    {
    }

    ~WorkerThread()
    {
        SQUEEZE_TRACE();
        wait_for_task();
        state.store(State::Stopping, std::memory_order::relaxed);
        state.notify_one();
        internal.join();
        SQUEEZE_TRACE("Finished");
    }

    WorkerThread(const WorkerThread&) = delete;
    WorkerThread(WorkerThread&&) = delete;

    WorkerThread& operator=(const WorkerThread&) = delete;
    WorkerThread& operator=(WorkerThread&&) = delete;

    bool try_assign_task(Task& task)
    {
        SQUEEZE_TRACE();
        if (!task)
            throw Exception<ThreadPool>("invalid task");

        State expected = State::Idle;
        if (!state.compare_exchange_weak(expected, State::Starting,
                    std::memory_order::acquire, std::memory_order::relaxed))
            return false;
        this->task.swap(task);
        state.store(State::Running, std::memory_order::release);
        state.notify_one();
        SQUEEZE_TRACE("Task assign succeeded");
        return true;
    }

    void wait_for_task() const noexcept
    {
        state.wait(State::Running, std::memory_order::acquire);
    }

private:
    void run()
    {
        SQUEEZE_TRACE();
        while (true) {
            state.wait(State::Idle, std::memory_order::acquire);
            state.wait(State::Starting, std::memory_order::acquire);

            if (state.load(std::memory_order::relaxed) == State::Stopping)
                break;

            SQUEEZE_TRACE("Starting task");
            task();
            SQUEEZE_TRACE("Finished task");
            state.store(State::Idle, std::memory_order::release);
            state.notify_one();
        }

        auto state_to_str = [](State state)
        {
            switch (state) {
            case State::Idle:
                return "idle";
            case State::Starting:
                return "starting";
            case State::Running:
                return "running";
            case State::Stopping:
                return "stopping";
            default:
                return "<unknown>";
            }
        };

        SQUEEZE_DEBUG("curr_state={}", state_to_str(state));
        SQUEEZE_TRACE("Returning from worker thread");
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
