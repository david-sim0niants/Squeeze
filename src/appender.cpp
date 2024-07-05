#include "squeeze/appender.h"

#include "squeeze/logging.h"
#include "squeeze/utils/overloaded.h"
#include "squeeze/utils/defer.h"
#include "squeeze/utils/defer_macros.h"
#include "squeeze/utils/iterator.h"
#include "squeeze/utils/io.h"

namespace squeeze {

Appender::Context::Context(unsigned concurrency)
    : thread_pool(concurrency), encoder_pool(thread_pool)
{
}

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::Appender::"

Appender::Appender(std::ostream& target) : target(target)
{
}

Appender::~Appender() = default;

void Appender::will_append(std::unique_ptr<EntryInput>&& entry_input, Error<Appender> *err)
{
    owned_entry_inputs.push_back(std::move(entry_input));
    will_append(*owned_entry_inputs.back(), err);
}

void Appender::will_append(EntryInput& entry_input, Error<Appender> *err)
{
    SQUEEZE_TRACE("Will append {}", entry_input.get_path());
    future_appends.emplace_back(entry_input, err);
}

void Appender::perform_appends(unsigned concurrency)
{
    SQUEEZE_TRACE();

    if (future_appends.empty()) {
        SQUEEZE_TRACE("No entry to append, exiting");
        return;
    }

    Context context(concurrency);
    {
        std::future<void> future_void = std::async(std::launch::async,
            std::mem_fn(&Appender::perform_scheduled_appends), this, std::ref(context.scheduler));
        schedule_appends(context);
        SQUEEZE_TRACE("Waiting for scheduled append tasks to complete");
        future_void.get();
    }
    SQUEEZE_TRACE("Stream put pointer at: {}", static_cast<long long>(target.tellp()));
}

Error<Appender> Appender::append(EntryInput& entry_input)
{
    Error<Appender> err;
    will_append(entry_input, &err);
    perform_appends();
    return err;
}

void Appender::schedule_appends(Context& context)
{
    for (auto& future_append : future_appends)
        if (auto e = schedule_append(context, future_append))
            if (future_append.error)
                *future_append.error = {"failed appending entry", e.report()};
    context.scheduler.finalize();
}

Error<Appender> Appender::schedule_append(Context& context, FutureAppend& future_append)
{
    SQUEEZE_TRACE();

    EntryHeader entry_header;
    EntryInput::ContentType content;

    auto e = future_append.entry_input.init(entry_header, content);
    DEFER( future_append.entry_input.deinit(); );
    if (e) {
        SQUEEZE_ERROR("Failed initializing entry input");
        return {"failed initializing entry input", e.report()};
    }

    SQUEEZE_DEBUG("entry_header={}", utils::stringify(entry_header));

    CompressionParams compression = entry_header.compression;
    context.scheduler.schedule_entry_append(std::move(entry_header), future_append.error);

    return std::visit(utils::Overloaded {
            [&context, &compression](std::istream *stream)
            {
                return schedule_append_stream(context, compression, *stream);
            },
            [&context, &compression](const std::string& str)
            {
                return schedule_append_string(context, compression, str);
            },
            [](std::monostate state) -> Error<Appender>
            {
                return success;
            },
        }, content
    );
}

Error<Appender> Appender::schedule_append_stream(
        Context& context, const CompressionParams& compression, std::istream& stream)
{
    SQUEEZE_TRACE("Scheduling append ");
    return compression.method == compression::CompressionMethod::None ?
        schedule_buffer_appends(context.scheduler, stream)
        :
        schedule_future_buffer_appends(context, compression, stream)
    ;
}

Error<Appender> Appender::schedule_append_string(
        Context& context, const CompressionParams& compression, const std::string& str)
{
    context.scheduler.schedule_string_append(std::string(str));
    return success;
}

Error<Appender> Appender::schedule_buffer_appends(AppendScheduler& scheduler, std::istream& stream)
{
    Buffer buffer(BUFSIZ);
    while (true) {
        stream.read(reinterpret_cast<char *>(buffer.data()), BUFSIZ);
        if (utils::validate_stream_bad(stream)) {
            scheduler.schedule_error_raise({"output read error", ErrorCode::from_current_errno().report()});
            break;
        }
        buffer.resize(stream.gcount());

        if (buffer.size() == BUFSIZ) {
            scheduler.schedule_buffer_append(Buffer(buffer));
        } else {
            if (not buffer.empty())
                scheduler.schedule_buffer_append(std::move(buffer));
            break;
        }
    }
    return success;
}

Error<Appender> Appender::schedule_future_buffer_appends(
        Context& context, const CompressionParams& compression, std::istream& stream)
{
    auto e = context.encoder_pool.schedule_stream_encode(stream, compression,
            utils::FunctionOutputIterator {
                [&context](std::future<Buffer>&& future_buffer)
                {
                    context.scheduler.schedule_buffer_append(std::move(future_buffer));
                }
            }
        );

    if (e.failed())
        context.scheduler.schedule_error_raise(std::move(e));

    return success;
}

}
