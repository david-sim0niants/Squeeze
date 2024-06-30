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

class EncoderPool {
private:
    struct Task;

public:
    explicit EncoderPool(misc::ThreadPool& thread_pool);
    ~EncoderPool();

    std::future<Buffer> schedule_buffer_encode(Buffer&& input, const CompressionParams& compression);

    template<std::output_iterator<Buffer> It>
    Error<> schedule_stream_encode(std::istream& stream, const CompressionParams& compression, It it)
    {
        std::future<Buffer> future_output; Error<> e = success;
        while ((e = schedule_stream_encode_step(future_output, stream, compression)).successful()
               and future_output.valid()) {
            *it = std::move(future_output); ++it;
        }
        return e;
    }

    inline void finalize()
    {
        scheduler.finalize();
    }

private:
    Error<> schedule_stream_encode_step(std::future<Buffer>& future_output,
            std::istream& stream, const CompressionParams& compression);

    misc::ThreadPool& thread_pool;
    misc::TaskScheduler<Task> scheduler;
};

template<std::input_iterator In, std::output_iterator<std::byte> Out>
void encode_chunk(In in, In in_end, Out out, const CompressionParams& compression)
{
    std::copy(in, in_end, out); // temporary
}

}
