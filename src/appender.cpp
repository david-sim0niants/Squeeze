#include "squeeze/appender.h"

#include "squeeze/logging.h"
#include "squeeze/utils/overloaded.h"
#include "squeeze/utils/defer.h"
#include "squeeze/utils/defer_macros.h"
#include "squeeze/utils/iterator.h"
#include "squeeze/utils/io.h"
#include "squeeze/misc/singleton.h"

namespace squeeze {

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

bool Appender::perform_appends()
{
    SQUEEZE_TRACE();

    if (future_appends.empty()) {
        SQUEEZE_TRACE("No entry to append, exiting");
        return true;
    }

    std::future<bool> fut_succeeded = std::async(std::launch::async,
                                                 [this]{ return perform_scheduled_appends();});
    bool succeeded = schedule_appends();

    SQUEEZE_TRACE("Waiting for scheduled append tasks to complete");
    succeeded = fut_succeeded.get() && succeeded;

    SQUEEZE_TRACE("Stream put pointer at: {}", static_cast<long long>(target.tellp()));
    return succeeded;
}

Error<Appender> Appender::append(EntryInput& entry_input)
{
    Error<Appender> err;
    will_append(entry_input, &err);
    perform_appends();
    return err;
}

bool Appender::schedule_appends()
{
    bool succeeded = true;
    DEFER( scheduler.finalize() );
    for (auto& future_append : future_appends)
        succeeded = schedule_append(future_append) && succeeded;
    return succeeded;
}

bool Appender::schedule_append(FutureAppend& future_append)
{
    SQUEEZE_TRACE();

    EntryHeader entry_header;
    EntryInput::ContentType content;

    auto e = future_append.entry_input.init(entry_header, content);
    DEFER( future_append.entry_input.deinit(); );
    if (e) {
        SQUEEZE_ERROR("Failed initializing entry input");
        if (future_append.error)
            *future_append.error = {
                "failed scheduling entry append because failed initializing the entry input",
                e.report()
            };
        return false;
    }

    SQUEEZE_DEBUG("entry_header={}", utils::stringify(entry_header));

    CompressionParams compression = entry_header.compression;
    scheduler.schedule_entry_append(std::move(entry_header), future_append.error);

    return std::visit(utils::Overloaded {
            [this, &compression](std::istream *stream)
            {
                return schedule_append_stream(compression, *stream);
            },
            [this, &compression](const std::string& str)
            {
                return schedule_append_string(compression, str);
            },
            [](std::monostate state)
            {
                return true;
            },
        }, content
    );
}

bool Appender::schedule_append_stream(const CompressionParams& compression, std::istream& stream)
{
    SQUEEZE_TRACE("Scheduling append ");
    return compression.method == compression::CompressionMethod::None ?
        schedule_buffer_appends(stream)
        :
        schedule_future_buffer_appends(compression, stream)
    ;
}

bool Appender::schedule_append_string(const CompressionParams& compression, const std::string& str)
{
    scheduler.schedule_string_append(std::string(str));
    return true;
}

bool Appender::schedule_buffer_appends(std::istream& stream)
{
    Buffer buffer(BUFSIZ);
    while (true) {
        stream.read(reinterpret_cast<char *>(buffer.data()), BUFSIZ);
        if (utils::validate_stream_fail(stream)) [[unlikely]] {
            scheduler.schedule_error_raise("output read error");
            return false;
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
    return true;
}

bool Appender::schedule_future_buffer_appends(
        const CompressionParams& compression, std::istream& stream)
{
    auto e = get_encoder_pool().schedule_stream_encode(stream, compression,
            utils::FunctionOutputIterator {
                [this](std::future<EncodedBuffer>&& future_buffer)
                {
                    scheduler.schedule_buffer_append(std::move(future_buffer));
                }
            }
        );
    if (e) {
        scheduler.schedule_error_raise(std::move(e));
        return false;
    }
    return true;
}

}
