#include "squeeze/remover.h"

#include "squeeze/logging.h"
#include "squeeze/utils/io.h"

namespace squeeze {

struct Remover::FutureRemove {
    mutable std::string path;
    uint64_t pos, len;
    Error<Remover> *error;

    FutureRemove(std::string&& path, uint64_t pos, uint64_t len, Error<Remover> *error)
        : path(std::move(path)), pos(pos), len(len), error(error)
    {}
};

inline bool Remover::FutureRemoveCompare::operator()(const FutureRemove& a, const FutureRemove& b)
{
    return a.pos > b.pos;
}

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::Remover::"

Remover::Remover(std::iostream& target) : target(target)
{
    future_removes.emplace(std::string(), EntryIterator::npos, EntryIterator::npos, nullptr);
}

Remover::~Remover() = default;

void Remover::will_remove(const EntryIterator& it, Error<Remover> *err)
{
    SQUEEZE_TRACE("Will remove {}", it->second.path);
    future_removes.emplace(std::string(it->second.path), it->first, it->second.get_encoded_full_size(), err);
}

Error<Remover> Remover::remove(const EntryIterator& it)
{
    Error<Remover> err;
    will_remove(it, &err);
    perform_removes();
    return err;
}

void Remover::perform_removes()
{
    SQUEEZE_TRACE("Removing {} entries", future_removes.size() - 1);

    // linear-time algorithm for removing multiple chunks of data from the stream at once

    target.seekp(0, std::ios_base::end);
    const uint64_t initial_endp = target.tellp();
    uint64_t gap_len = 0; // gap length: the increasing size of the gap that gets pushed to the right

    // future remove operations are stored in a priority queue based on their positions

    while (future_removes.size() > 1) {
        std::string path;
        future_removes.top().path.swap(path);
        uint64_t pos = future_removes.top().pos;
        uint64_t len = future_removes.top().len;
        Error<Remover> *error = future_removes.top().error;

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
         * In other words, each subsequent gap is pushed to the right, merging it with the
         * next gap and so on. Eventually all the gaps will be merged together and pushed to
         * the end of stream. */

        // the position of the following non-gap data
        const uint64_t mov_pos = pos + len;
        // the length of the following non-gap data
        const uint64_t mov_len = std::min(next_pos, initial_endp) - mov_pos;
        // pos - gap_len is the destination of the following non-gap data to move to
        utils::iosmove(target, pos - gap_len, mov_pos, mov_len); // do the move

        if (utils::validate_stream_fail(target)) {
            SQUEEZE_ERROR("Target output stream failed");
            if (error)
                *error = {"failed removing entry '" + path + '\'',
                          Error<>("target output stream failed").report()};
        }

        gap_len += len; // increase the gap size
    }
    assert(future_removes.size() == 1 && "The last element in the priority queue of future removes must remain after performing all the remove operations.");
    target.seekp(initial_endp - gap_len); // point to the new end of the stream
}

}
