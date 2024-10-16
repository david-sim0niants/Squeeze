// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <future>
#include <concepts>

#include "encode.h"
#include "misc/thread_pool.h"
#include "misc/task_scheduler.h"

namespace squeeze {

using EncodedBuffer = std::pair<Buffer, EncodeStat>;

class EncoderPool {
public:
    using Stat = EncodeStat;

private:
    struct Task;

public:
    EncoderPool();
    explicit EncoderPool(misc::ThreadPool& thread_pool);
    ~EncoderPool();

    std::future<EncodedBuffer> schedule_buffer_encode(Buffer&& input, const CompressionParams& compression);

    template<std::output_iterator<Buffer> It>
    Stat schedule_stream_encode(std::istream& stream, const CompressionParams& compression, It it)
    {
        std::future<EncodedBuffer> future_output; Stat stat = success;
        while ((stat = std::move(schedule_stream_encode_step(future_output, stream, compression)))
                and future_output.valid()) {
            *it = std::move(future_output); ++it;
        }
        return stat;
    }

    void wait_for_tasks() noexcept;

private:
    Stat schedule_stream_encode_step(std::future<EncodedBuffer>& future_output,
            std::istream& stream, const CompressionParams& compression);
    void try_another_thread();
    void threaded_task_run();

    misc::ThreadPool& thread_pool;
    misc::TaskScheduler<Task> scheduler;
    std::atomic_size_t nr_running_threads = 0;
};

}