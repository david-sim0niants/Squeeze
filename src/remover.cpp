// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "squeeze/remover.h"

#include "squeeze/logging.h"
#include "squeeze/utils/io.h"

#include <cassert>

namespace squeeze {

struct Remover::FutureRemove {
    mutable std::string path;
    uint64_t pos, len;
    Stat *status;

    FutureRemove(std::string&& path, uint64_t pos, uint64_t len, Stat *status)
        : path(std::move(path)), pos(pos), len(len), status(status)
    {
    }
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

void Remover::will_remove(const EntryIterator& it, Stat *stat)
{
    SQUEEZE_TRACE("Will remove {}", it->second.path);
    future_removes.emplace(std::string(it->second.path), it->first, it->second.get_encoded_full_size(), stat);
}

Remover::Stat Remover::remove(const EntryIterator& it)
{
    Stat stat;
    will_remove(it, &stat);
    perform_removes();
    return stat;
}

bool Remover::perform_removes()
{
    SQUEEZE_TRACE("Removing {} entries", future_removes.size() - 1);

    // linear-time algorithm for removing multiple chunks of data from the stream at once

    target.seekp(0, std::ios_base::end);
    const uint64_t initial_endp = target.tellp();
    SQUEEZE_DEBUG("initial_endp={}", initial_endp);

    uint64_t gap_len = 0; // gap length: the increasing size of the gap that gets pushed to the right

    // future remove operations are stored in a priority queue based on their positions

    while (future_removes.size() > 1) {
        std::string path;
        future_removes.top().path.swap(path);
        uint64_t pos = future_removes.top().pos;
        uint64_t len = future_removes.top().len;
        Stat *stat = future_removes.top().status;

        SQUEEZE_INFO("Removing {}", path);

        uint64_t next_pos = pos;
        while (true) {
            /** There might be some "unpleasant" cases when a remove operation
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

        /** Removing a chunk of data is just moving another chunk of data, that is,
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
        Stat s = utils::iosmove(target, pos - gap_len, mov_pos, mov_len); // do the move

        if (s.failed()) [[unlikely]] {
            SQUEEZE_ERROR("Failed performing removes starting from '{}'", path);
            SQUEEZE_DEBUG("pos={}, gap_len={}, mov_pos={}, mov_len={}, len={}",
                          pos, gap_len, mov_pos, mov_len, len);

            if (stat)
                *stat = {"failed removing '" + path + '\'', s};

            while (future_removes.size() > 1)
                future_removes.pop();

            return false;
        }

        gap_len += len; // increase the gap size
    }

    assert(future_removes.size() == 1 && "The last element in the priority queue of future removes must remain after performing all the remove operations.");
    target.seekp(initial_endp - gap_len); // point to the new end of the stream
    return true;
}

}
