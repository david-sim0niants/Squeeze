#include "squeeze/encoder.h"

#include "squeeze/logging.h"
#include "squeeze/utils/io.h"
#include "squeeze/utils/defer.h"
#include "squeeze/utils/defer_macros.h"

namespace squeeze {

struct EncoderPool::Task {
    Buffer input;
    CompressionParams compression;
    std::promise<Buffer> output_promise;

    Task(Buffer&& input, CompressionParams&& compression)
        : input(std::move(input)), compression(std::move(compression))
    {}

    void operator()()
    {
        Buffer output;
        encode_chunk(input.begin(), input.size(), std::back_insert_iterator(output), compression);
        output_promise.set_value(std::move(output));
    }
};

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::EncoderPool::"

EncoderPool::EncoderPool(misc::ThreadPool& thread_pool) : thread_pool(thread_pool)
{
}

EncoderPool::~EncoderPool()
{
    finalize();
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

std::future<Buffer>
EncoderPool::schedule_buffer_encode(Buffer&& input, const CompressionParams& compression)
{
    SQUEEZE_TRACE();
    Task task {std::move(input), CompressionParams(compression)};
    auto future_output = task.output_promise.get_future();
    scheduler.schedule(std::move(task));
    try_another_thread();
    return future_output;
}

Error<> EncoderPool::schedule_stream_encode_step(std::future<Buffer>& future_output,
        std::istream& stream, const CompressionParams& compression)
{
    SQUEEZE_TRACE();
    constexpr size_t _tmp_buffer_size = 8192;

    Buffer buffer(_tmp_buffer_size);
    stream.read(reinterpret_cast<char *>(buffer.data()), _tmp_buffer_size);
    if (utils::validate_stream_bad(stream))
        return {"output read error", ErrorCode::from_current_errno().report()};

    buffer.resize(stream.gcount());

    future_output = buffer.size() == _tmp_buffer_size ?
        schedule_buffer_encode(Buffer(buffer), compression)
        :
        buffer.empty() ?
            std::future<Buffer>{}
            :
            schedule_buffer_encode(std::move(buffer), compression)
        ;
    return success;
}

void EncoderPool::try_another_thread()
{
    thread_pool.try_assign_task(
            [this]()
            {
                nr_running_threads.fetch_add(1, std::memory_order::acquire);
                DEFER ( nr_running_threads.fetch_sub(1, std::memory_order::release);
                        nr_running_threads.notify_all(); );
                scheduler.run();
            }
        );
}

}
