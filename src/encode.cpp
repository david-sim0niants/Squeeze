#include "squeeze/encode.h"

#include "squeeze/logging.h"

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
        encode_chunk(input.begin(), input.end(), std::back_insert_iterator(output), compression);
        output_promise.set_value(std::move(output));
    }
};

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::EncoderPool::"

EncoderPool::EncoderPool(misc::ThreadPool& thread_pool) : thread_pool(thread_pool)
{
}

EncoderPool::~EncoderPool() = default;

std::future<Buffer>
EncoderPool::schedule_buffer_encode(Buffer&& input, const CompressionParams& compression)
{
    Task task {std::move(input), CompressionParams(compression)};
    auto future_output = task.output_promise.get_future();
    scheduler.schedule(std::move(task));
    thread_pool.try_assign_task(
            [&scheduler = this->scheduler]()
            {
                SQUEEZE_TRACE("Scheduler start");
                scheduler.run();
                SQUEEZE_TRACE("Scheduler finish");
            }
        );
    return future_output;
}

Error<> EncoderPool::schedule_stream_encode_step(std::future<Buffer>& future_output,
        std::istream& stream, const CompressionParams& compression)
{
    constexpr size_t _tmp_buffer_size = 8192;

    Buffer buffer(_tmp_buffer_size);
    stream.read(reinterpret_cast<char *>(buffer.data()), _tmp_buffer_size);
    if (stream.bad()) {
        stream.clear();
        return {"output read error", ErrorCode::from_current_errno().report()};
    }

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

}
