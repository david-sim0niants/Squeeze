#pragma once

#include <thread>
#include <istream>
#include <future>
#include <concepts>

#include "common.h"
#include "error.h"
#include "compression/params.h"
#include "misc/thread_pool.h"
#include "misc/task_scheduler.h"

namespace squeeze {

using compression::CompressionParams;

using EncodedBuffer = std::pair<Buffer, Error<>>;

class EncoderPool {
private:
    struct Task;

public:
    EncoderPool();
    explicit EncoderPool(misc::ThreadPool& thread_pool);
    ~EncoderPool();

    std::future<EncodedBuffer> schedule_buffer_encode(Buffer&& input, const CompressionParams& compression);

    template<std::output_iterator<Buffer> It>
    Error<> schedule_stream_encode(std::istream& stream, const CompressionParams& compression, It it)
    {
        std::future<EncodedBuffer> future_output; Error<> e = success;
        while ((e = schedule_stream_encode_step(future_output, stream, compression)).successful()
               and future_output.valid()) {
            *it = std::move(future_output); ++it;
        }
        return e;
    }

    void wait_for_tasks() noexcept;

private:
    Error<> schedule_stream_encode_step(std::future<EncodedBuffer>& future_output,
            std::istream& stream, const CompressionParams& compression);
    void try_another_thread();
    void threaded_task_run();

    misc::ThreadPool& thread_pool;
    misc::TaskScheduler<Task> scheduler;
    std::atomic_size_t nr_running_threads = 0;
};

Error<> encode_buffer(const Buffer& in, Buffer& out, const CompressionParams& compression);

Error<> encode(std::istream& in, std::size_t size, std::ostream& out,
               const compression::CompressionParams& compression);

}
