#pragma once

#include <thread>
#include <istream>
#include <future>
#include <concepts>

#include "common.h"
#include "status.h"
#include "compression/params.h"
#include "misc/thread_pool.h"
#include "misc/task_scheduler.h"

namespace squeeze {

using compression::CompressionParams;
using EncoderStat = StatStr;

using EncodedBuffer = std::pair<Buffer, EncoderStat>;

class EncoderPool {
public:
    using Stat = EncoderStat;

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

/* Encode single buffer using the compression info provided. */
EncoderStat encode_buffer(const Buffer& in, Buffer& out, const CompressionParams& compression);

/* Encode a char stream of a given size into another char stream using the compression info provided.
 * Unlike the EncoderPool, doesn't use multithreading. */
EncoderStat encode(std::istream& in, std::size_t size, std::ostream& out,
               const compression::CompressionParams& compression);

}
