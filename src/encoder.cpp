#include "squeeze/encoder.h"

#include "squeeze/compression/compression.h"
#include "squeeze/logging.h"
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

EncoderStat EncoderPool::schedule_stream_encode_step(std::future<EncodedBuffer>& future_output,
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
    SQUEEZE_DEBUG("Number of running threads: {}", nr_running_threads);
    nr_running_threads.fetch_add(1, std::memory_order::acquire);
    DEFER ( nr_running_threads.fetch_sub(1, std::memory_order::release);
            nr_running_threads.notify_all(); );
    scheduler.run<misc::TaskRunPolicy::NoWait>();
}

EncoderStat encode_buffer(const Buffer& in, Buffer& out, const CompressionParams& compression)
{
    if (compression.method == compression::CompressionMethod::None) {
        std::copy(in.begin(), in.end(), std::back_inserter(out));
        return success;
    }

    using namespace compression;
    using enum compression::CompressionFlags;

    const bool final_block = in.size() < compression::get_block_size(compression);
    const CompressionFlags flags = utils::switch_flag(FinalBlock, final_block);
    return std::get<2>(compress(in.begin(), in.end(), compression, flags, std::back_inserter(out))).status;
}

EncoderStat encode(std::istream& in, std::size_t size, std::ostream& out,
            const compression::CompressionParams& compression)
{
    SQUEEZE_TRACE();

    if (compression.method == compression::CompressionMethod::None) {
        SQUEEZE_TRACE("Compression method is none, plain copying...");
        EncoderStat s = utils::ioscopy(in, in.tellg(), out, out.tellp(), size);
        return s ? success : EncoderStat{"failed copying stream", s};
    }

    const std::size_t buffer_size = compression::get_block_size(compression);

    Buffer inbuf(buffer_size);
    Buffer outbuf;

    while (size) {
        inbuf.resize(std::min(buffer_size, size));
        in.read(inbuf.data(), inbuf.size());
        size -= inbuf.size();
        if (utils::validate_stream_fail(in)) [[unlikely]] {
            SQUEEZE_ERROR("Input read error");
            return "input read error";
        }
        inbuf.resize(in.gcount());

        auto s = encode_buffer(inbuf, outbuf, compression);
        if (s.failed()) [[unlikely]] {
            SQUEEZE_ERROR("Failed encoding buffer");
            return {"failed encoding buffer", s};
        }

        out.write(outbuf.data(), outbuf.size());
        if (utils::validate_stream_fail(out)) [[unlikely]] {
            SQUEEZE_ERROR("output write error");
            return "output write error";
        }
        outbuf.clear();
    }

    return success;
}

}
