#pragma once

#include <optional>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace squeeze::misc {

template<typename T>
class ThreadSafeQueue {
public:
    void finish()
    {
        std::scoped_lock q_lock (mutex);
        finish_flag = true;
        cv.notify_all();
    }

    void push(T&& value)
    {
        std::scoped_lock q_lock (mutex);
        internal_q.push(std::move(value));
        cv.notify_one();
    }

    void push(const T& value)
    {
        std::scoped_lock q_lock (mutex);
        internal_q.push(value);
        cv.notify_one();
    }

    void emplace(auto&&... args)
    {
        std::scoped_lock q_lock (mutex);
        internal_q.emplace(std::forward<decltype(args)>(args)...);
        cv.notify_one();
    }

    std::optional<T> try_pop()
    {
        std::scoped_lock q_lock(mutex);
        if (internal_q.empty())
            return std::nullopt;
        T value = std::move(internal_q.front());
        internal_q.pop();
        return std::move(value);
    }

    std::optional<T> try_wait_and_pop()
    {
        std::unique_lock q_lock(mutex);
        cv.wait(q_lock, [this](){ return !internal_q.empty() or finish_flag; });
        if (internal_q.empty())
            return std::nullopt;
        T value = std::move(internal_q.front());
        internal_q.pop();
        return std::move(value);
    }

private:
    std::queue<T> internal_q;
    bool finish_flag = false;
    std::mutex mutex;
    std::condition_variable cv;
};

}
