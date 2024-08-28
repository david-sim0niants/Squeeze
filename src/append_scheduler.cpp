#include "squeeze/append_scheduler.h"

#include "squeeze/logging.h"
#include "squeeze/utils/io.h"
#include "squeeze/utils/overloaded.h"

namespace squeeze {

/* Abstract block appender. */
class BlockAppender {
public:
    virtual Error<> run(std::ostream& target) = 0;
};

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::BufferAppender::"

/* Buffer appender task. */
class BufferAppender final : public BlockAppender {
public:
    explicit BufferAppender(Buffer&& buffer) : buffer(std::move(buffer))
    {
    }

    Error<> run(std::ostream& target)
    {
        SQUEEZE_TRACE("Got a buffer with size={}", buffer.size());
        target.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
        if (utils::validate_stream_bad(target)) {
            SQUEEZE_ERROR("Failed appending buffer");
            return {"failed appending buffer", ErrorCode::from_current_errno().report()};
        }
        return success;
    }

private:
    Buffer buffer;
};

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::FutureBufferAppender::"

using FutureBuffer = std::future<std::pair<Buffer, Error<>>>;

/* Future buffer appender task. */
class FutureBufferAppender final : public BlockAppender {
public:
    explicit FutureBufferAppender(FutureBuffer&& future_buffer)
        : future_buffer(std::move(future_buffer))
    {
    }

    Error<> run(std::ostream& target)
    {
        SQUEEZE_TRACE("Waiting for future to complete.");
        const auto& [buffer, e] = future_buffer.get();
        if (e) {
            SQUEEZE_ERROR("Buffer encoding failed");
            return {"buffer encoding failed", e.report()};
        }

        SQUEEZE_TRACE("Got a buffer with size={}", buffer.size());

        target.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
        if (utils::validate_stream_bad(target)) {
            SQUEEZE_ERROR("Failed appending buffer");
            return {"failed appending buffer", ErrorCode::from_current_errno().report()};
        }
        return success;
    }

private:
    FutureBuffer future_buffer;
};

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::ErrorRaiser::"

/* Error raiser task. */
class ErrorRaiser final : public BlockAppender {
public:
    ErrorRaiser(Error<>&& error) : error(std::move(error))
    {
    }

    Error<> run(std::ostream& target)
    {
        SQUEEZE_ERROR("Got an error: {}", error.report());
        return std::move(error);
    }

private:
    Error<> error;
};

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::StringAppender::"

/* String appender task. */
class StringAppender final : public BlockAppender {
public:
    StringAppender(std::string&& str) : str(std::move(str))
    {
    }

    Error<> run(std::ostream& target)
    {
        SQUEEZE_TRACE("Got a string: '{}'", str);

        target.write(str.data(), str.size() + 1);
        if (utils::validate_stream_bad(target)) {
            SQUEEZE_ERROR("Failed appending string");
            return {"failed appending string", ErrorCode::from_current_errno().report()};
        }

        return success;
    }

private:
    std::string str;
};

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::AppendScheduler::EntryAppendTask::"

EntryAppendScheduler::Task::Task(std::unique_ptr<BlockAppender>&& block_appender)
    : block_appender(std::move(block_appender))
{
}

EntryAppendScheduler::Task::~Task() = default;

inline Error<EntryAppendScheduler::Task> EntryAppendScheduler::Task::operator()(std::ostream& target)
{
    return block_appender->run(target);
}

EntryAppendScheduler::EntryAppendScheduler(EntryHeader entry_header, Error<> *error)
    : entry_header(entry_header), error(error)
{
}

EntryAppendScheduler::~EntryAppendScheduler()
{
    close();
}

inline void EntryAppendScheduler::schedule_error_raise(Error<>&& error)
{
    scheduler.schedule(std::make_unique<ErrorRaiser>(std::move(error)));
}

inline void EntryAppendScheduler::schedule_buffer_append(FutureBuffer&& future_buffer)
{
    scheduler.schedule(std::make_unique<FutureBufferAppender>(std::move(future_buffer)));
}

inline void EntryAppendScheduler::schedule_buffer_append(Buffer&& buffer)
{
    scheduler.schedule(std::make_unique<BufferAppender>(std::move(buffer)));
}

inline void EntryAppendScheduler::schedule_string_append(std::string&& str)
{
    scheduler.schedule(std::make_unique<StringAppender>(std::move(str)));
}

void EntryAppendScheduler::set_error(const Error<>& e)
{
    if (!error)
        return;
    if (e)
        *error = {"failed appending entry '" + entry_header.path + '\'', e.report()};
    else
        *error = success;
}

Error<> EntryAppendScheduler::run_internal(std::ostream& target)
{
    SQUEEZE_TRACE("Appending {}", entry_header.path);

    const std::streampos initial_pos = target.tellp();
    SQUEEZE_DEBUG("initial_pos = {}", static_cast<long long>(initial_pos));

    SQUEEZE_TRACE("Encoding entry_header = {}", utils::stringify(entry_header));
    auto ehe = EntryHeader::encode(target, entry_header);
    if (ehe) {
        target.seekp(initial_pos);
        SQUEEZE_ERROR("Failed encoding the entry header");
        return {"failed encoding the entry header", ehe.report()};
    }

    const std::streampos content_pos = target.tellp();

    SQUEEZE_TRACE("Running scheduled tasks");
    auto e = scheduler.run_till_error(target);
    if (e) {
        target.seekp(initial_pos);
        SQUEEZE_ERROR("Failed appending content");
        return {"failed appending content", e.report()};
    }

    const std::streampos final_pos = target.tellp();
    SQUEEZE_DEBUG("final_pos = {}", static_cast<long long>(final_pos));

    target.seekp(initial_pos);

    entry_header.content_size = final_pos - content_pos;
    SQUEEZE_DEBUG("Encoding entry_header.content_size={}", entry_header.content_size);
    e = EntryHeader::encode_content_size(target, entry_header.content_size);
    if (e) {
        target.seekp(initial_pos);
        SQUEEZE_ERROR("Failed encoding content size");
        return {"failed encoding content size", e.report()};
    }

    target.seekp(final_pos);

    return success;
}

AppendScheduler::Task::Task(EntryHeader entry_header, Error<> *error) noexcept
    : scheduler(std::make_unique<EntryAppendScheduler>(entry_header, error))
{
}

AppendScheduler::Task::~Task() = default;

void AppendScheduler::Task::operator()(std::ostream& target)
{
    SQUEEZE_TRACE();
    scheduler->run(target);
}

AppendScheduler::AppendScheduler() noexcept = default;

AppendScheduler::~AppendScheduler()
{
    finalize();
}

void AppendScheduler::schedule_entry_append(EntryHeader&& entry_header, Error<> *error)
{
    finalize_entry_append();
    scheduler.open();
    Task task {entry_header, error};
    last_entry_append_scheduler = task.scheduler.get();
    // last_entry_append_scheduler is safe to use until finalize() or finalize_entry_append() are called
    scheduler.schedule(std::move(task));
}

void AppendScheduler::schedule_error_raise(Error<>&& error)
{
    SQUEEZE_TRACE();
    assert(last_entry_append_scheduler != nullptr);
    last_entry_append_scheduler->schedule_error_raise(std::move(error));
}

void AppendScheduler::schedule_buffer_append(FutureBuffer&& future_buffer)
{
    SQUEEZE_TRACE("future_buffer");
    assert(last_entry_append_scheduler != nullptr);
    last_entry_append_scheduler->schedule_buffer_append(std::move(future_buffer));
}

void AppendScheduler::schedule_buffer_append(Buffer&& buffer)
{
    SQUEEZE_TRACE("buffer");
    assert(last_entry_append_scheduler != nullptr);
    last_entry_append_scheduler->schedule_buffer_append(std::move(buffer));
}

void AppendScheduler::schedule_string_append(std::string&& str)
{
    SQUEEZE_TRACE();
    assert(last_entry_append_scheduler != nullptr);
    last_entry_append_scheduler->schedule_string_append(std::move(str));
}

void AppendScheduler::finalize_entry_append() noexcept
{
    if (last_entry_append_scheduler)
        last_entry_append_scheduler->close();
    last_entry_append_scheduler = nullptr;
}

}

