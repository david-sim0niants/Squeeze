#include "squeeze/writer.h"

#include <cassert>

#include "squeeze/logging.h"
#include "squeeze/exception.h"
#include "squeeze/utils/io.h"
#include "squeeze/utils/fs.h"
#include "squeeze/encode.h"

namespace squeeze {

struct Writer::FutureRemove {
    mutable std::string path;
    uint64_t pos, len;
    Error<Writer> *error;

    FutureRemove(std::string&& path, uint64_t pos, uint64_t len, Error<Writer> *error)
        : path(std::move(path)), pos(pos), len(len), error(error)
    {}
};

inline bool Writer::FutureRemoveCompare::operator()(const FutureRemove& a, const FutureRemove& b)
{
    return a.pos > b.pos;
}

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::Writer::"

Writer::Writer(std::iostream& target) : Appender(target), target(target)
{
    future_removes.emplace(std::string(), ReaderIterator::npos, ReaderIterator::npos, nullptr);
}

Writer::~Writer() = default;

void Writer::will_remove(const ReaderIterator& it, Error<Writer> *err)
{
    SQUEEZE_TRACE("Will remove {}", it->second.path);
    future_removes.emplace(std::string(it->second.path), it->first, it->second.get_encoded_full_size(), err);
}

void Writer::write(unsigned concurrency)
{
    SQUEEZE_TRACE();
    if (future_appends.empty()) {
        SQUEEZE_TRACE("No entry to append, performing removes synchronously");
        perform_removes();
    } else {
        Appender::Context context(concurrency);
        auto future_void = std::async(std::launch::async,
            [this, &scheduler = context.scheduler]()
            {
                perform_removes();
                perform_scheduled_appends(scheduler);
            }
        );
        SQUEEZE_TRACE("Waiting for scheduled append tasks to complete");
        schedule_appends(context);
        future_void.get();
    }
    SQUEEZE_TRACE("Stream put pointer at: {}", static_cast<long long>(target.tellp()));
}

Error<Writer> Writer::remove(const ReaderIterator& it)
{
    Error<Writer> err;
    will_remove(it, &err);
    perform_removes();
    return err;
}

void Writer::perform_removes()
{
    SQUEEZE_TRACE("Removing {} entries", future_removes.size() - 1);

    // linear-time algorithm for removing multiple chunks of data from the stream at once

    target.seekp(0, std::ios_base::end);
    const uint64_t initial_endp = target.tellp();
    uint64_t rem_len = 0; // remove length: the increasing size of the "hole" that gets pushed right

    // future remove operations are stored in a priority queue based on their positions

    while (future_removes.size() > 1) {
        std::string path;
        future_removes.top().path.swap(path);
        uint64_t pos = future_removes.top().pos;
        uint64_t len = future_removes.top().len;
        Error<Writer> *error = future_removes.top().error;

        SQUEEZE_TRACE("Removing {}", path);

        uint64_t next_pos = pos;
        while (true) {
            /* There might be some "unpleasant" cases when a remove operation
             * at the same position has been registered multiple times.
             * To overcome this problem we just keep popping out the next remove
             * until its position is different. We will need the next remove position later. */
            future_removes.pop();
            next_pos = future_removes.top().pos;
            if (pos == next_pos) {
                SQUEEZE_WARN("More than one entry remove with the same position: {} | path: {}", pos, path);
                continue;
            } else {
                break;
            }
        };

        /* Removing a chunk of data is just moving another chunk of data, that is,
         * between the current one and the next chunk of data to remove, to the left,
         * concatenating it with the rest of the remaining data.
         * After each remove operation, the size of the "hole" (rem_len) is increased
         * and pushed right till the end of the stream. */

        // the position of the following non-removing data
        const uint64_t mov_pos = pos + len;
        // the length of the following non-removing data
        const uint64_t mov_len = std::min(next_pos, initial_endp) - mov_pos;
        // pos - rem_len is the destination of the following non-removing data to move to
        utils::iosmove(target, pos - rem_len, mov_pos, mov_len); // do the move

        if (utils::validate_stream_fail(target)) {
            SQUEEZE_ERROR("Target output stream failed");
            if (error)
                *error = {"failed removing entry '" + path + '\'', "target output stream failed"};
        }

        rem_len += len; // increase the "hole" size
    }
    assert(future_removes.size() == 1 && "The last element in the priority queue of future removes must remain after performing all the remove operations.");
    target.seekp(initial_endp - rem_len); // point to the new end of the stream
}

}
