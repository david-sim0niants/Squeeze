// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "squeeze/encoder_pool.h"

#include "squeeze/logging.h"
#include "squeeze/compression/config.h"
#include "squeeze/utils/io.h"
#include "squeeze/utils/defer.h"
#include "squeeze/utils/defer_macros.h"
#include "squeeze/misc/singleton.h"

namespace squeeze {

struct EncoderPool::Task {
    Buffer input;
    CompressionParams compression;
    std::promise<EncodedBuffer> output_promise;

    Task(Buffer&& input, CompressionParams&& compression)
        : input(std::move(input)), compression(std::move(compression))
    {
    }

    void operator()()
    {
        try {
            Buffer output;
            Stat stat = encode_buffer(input, output, compression);
            output_promise.set_value(std::make_pair(std::move(output), std::move(stat)));
        } catch (...) {
            output_promise.set_exception(std::current_exception());
        }
    }
};

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::EncoderPool::"

EncoderPool::EncoderPool() : thread_pool(misc::Singleton<misc::ThreadPool>::instance())
{
}

EncoderPool::EncoderPool(misc::ThreadPool& thread_pool) : thread_pool(thread_pool)
{
}

EncoderPool::~EncoderPool()
{
    scheduler.close();
    wait_for_tasks();
}

void EncoderPool::wait_for_tasks() noexcept
{
    while (true) {
        size_t curr_nr_running_threads = nr_running_threads.load(std::memory_order::relaxed);
        if (curr_nr_running_threads == 0) {
            if (scheduler.get_nr_tasks_left() == 0)
                break;
            else
                try_another_thread();
        }
        nr_running_threads.wait(curr_nr_running_threads, std::memory_order::acquire);
    }
}

std::future<EncodedBuffer> EncoderPool::
    schedule_buffer_encode(Buffer&& input, const CompressionParams& compression)
{
    SQUEEZE_TRACE();
    Task task {std::move(input), CompressionParams(compression)};
    auto future_output = task.output_promise.get_future();
    scheduler.schedule(std::move(task));
    try_another_thread();
    return future_output;
}

EncodeStat EncoderPool::schedule_stream_encode_step(std::future<EncodedBuffer>& future_output,
        std::istream& stream, const CompressionParams& compression)
{
    SQUEEZE_TRACE();
    const size_t buffer_size = compression.method == compression::CompressionMethod::None ?
        BUFSIZ : compression::get_block_size(compression);

    Buffer buffer(buffer_size);
    stream.read(reinterpret_cast<char *>(buffer.data()), buffer_size);
    if (utils::validate_stream_fail(stream)) [[unlikely]]
        return "output read error";

    buffer.resize(stream.gcount());

    future_output = buffer.size() == buffer_size ?
        schedule_buffer_encode(Buffer(buffer), compression)
        :
        buffer.empty() ?
            std::future<EncodedBuffer>{}
            :
            schedule_buffer_encode(std::move(buffer), compression)
        ;
    return success;
}

void EncoderPool::try_another_thread()
{
    thread_pool.try_assign_task([this](){ threaded_task_run(); });
}

void EncoderPool::threaded_task_run()
{
    SQUEEZE_DEBUG("Number of running threads: {}", nr_running_threads.load(std::memory_order::acquire));
    nr_running_threads.fetch_add(1, std::memory_order::acquire);
    DEFER ( nr_running_threads.fetch_sub(1, std::memory_order::release);
            nr_running_threads.notify_all(); );
    scheduler.run<misc::TaskRunPolicy::NoWait>();
}

}