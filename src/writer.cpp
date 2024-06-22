#include "squeeze/writer.h"
#include "writer_internal.h"

#include "squeeze/logging.h"
#include "squeeze/exception.h"
#include "squeeze/utils/io.h"
#include "squeeze/utils/fs.h"

#include <cassert>

namespace squeeze {

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::Writer::"

Writer::Writer(std::iostream& target) : target(target)
{
    future_removes.emplace(std::string(), ReaderIterator::npos, ReaderIterator::npos, nullptr);
}

Writer::~Writer() = default;

void Writer::will_append(std::unique_ptr<EntryInput>&& entry_input, Error<Writer> *err)
{
    owned_entry_inputs.push_back(std::move(entry_input));
    will_append(*owned_entry_inputs.back(), err);
}

void Writer::will_append(EntryInput& entry_input, Error<Writer> *err)
{
    SQUEEZE_TRACE("Will append {}", entry_input.get_path());
    future_appends.emplace_back(entry_input, err);
}

void Writer::will_remove(const ReaderIterator& it, Error<Writer> *err)
{
    SQUEEZE_TRACE("Will remove {}", it->second.path);
    future_removes.emplace(std::string(it->second.path), it->first, it->second.get_encoded_full_size(), err);
}

void Writer::write()
{
    SQUEEZE_TRACE();
    perform_removes();
    perform_appends();
    SQUEEZE_DEBUG("Stream put pointer at: {}", static_cast<long long>(target.tellp()));
}

Error<Writer> Writer::append(EntryInput& entry_input)
{
    Error<Writer> err;
    will_append(entry_input, &err);
    write();
    return err;
}

Error<Writer> Writer::remove(const ReaderIterator&& it)
{
    Error<Writer> err;
    will_remove(it, &err);
    write();
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

        if (target.fail()) {
            SQUEEZE_ERROR("Target output stream failed");
            if (error)
                *error = {"failed removing entry '" + path + '\'', "target output stream failed"};
            target.clear();
        }

        rem_len += len; // increase the "hole" size
    }
    assert(future_removes.size() == 1 && "The last element in the priority queue of future removes must remain after performing all the remove operations.");
    target.seekp(initial_endp - rem_len); // point to the new end of the stream
}

void Writer::perform_appends()
{
    SQUEEZE_TRACE("Appending {} entries", future_appends.size());

    for (auto& future_append : future_appends) {
        auto e = perform_append(future_append.entry_input);
        if (future_append.error) {
            if (e)
                *future_append.error = {"failed appending entry '" + future_append.entry_input.get_path() + '\'', e.report()};
            else
                *future_append.error = success;
        }
    }
    future_appends.clear();
    owned_entry_inputs.clear();
}

Error<Writer> Writer::perform_append(EntryInput& entry_input)
{
    SQUEEZE_TRACE("Appending {}", entry_input.get_path());

    const std::streampos initial_pos = target.tellp();

    EntryInput::ContentType content = std::monostate();
    EntryHeader entry_header {};

    auto e = entry_input.init(entry_header, content);
    if (e) {
        SQUEEZE_ERROR("Failed initializing entry input");
        return {"failed initializing entry input", e.report()};
    }

    SQUEEZE_DEBUG("initial_pos = {}", static_cast<long long>(initial_pos));

    SQUEEZE_TRACE("Encoding entry_header = {}", utils::stringify(entry_header));
    auto ehe = EntryHeader::encode(target, entry_header);
    if (ehe) {
        target.seekp(initial_pos);
        SQUEEZE_ERROR("Failed encoding the entry header");
        return {"failed encoding the entry header", ehe.report()};
    }

    e = perform_append_content(entry_header, content);

    const std::streampos final_pos = target.tellp();
    target.seekp(initial_pos);

    if (e) {
        SQUEEZE_ERROR("Failed appending entry content");
        return {"failed appending entry content", e.report()};
    }

    SQUEEZE_TRACE("Encoding entry_header.content_size={}", entry_header.content_size);
    e = EntryHeader::encode_content_size(target, entry_header.content_size);
    if (e) {
        SQUEEZE_ERROR("Failed encoding entry_header.content_size");
        target.seekp(initial_pos);
        return "failed encoding entry_header.content_size";
    }

    SQUEEZE_DEBUG("final_pos = {}", static_cast<long long>(final_pos));
    target.seekp(final_pos);

    return success;
}

Error<Writer> Writer::perform_append_content(EntryHeader& entry_header, const EntryInput::ContentType& content)
{
    SQUEEZE_DEBUG("target.fail() = {}", target.fail());
    if (std::holds_alternative<std::istream *>(content)) {
        SQUEEZE_TRACE("Entry content is a stream, appending...");
        return perform_append_stream(entry_header, *std::get<std::istream *>(content));
    } else if (std::holds_alternative<std::string>(content)) {
        SQUEEZE_TRACE("Entry content is a string, appending...");
        return perform_append_string(entry_header, std::get<std::string>(content));
    } else {
        SQUEEZE_TRACE("Entry content is empty");
        entry_header.content_size = 0;
        return success;
    }
}

Error<Writer> Writer::perform_append_stream(EntryHeader& entry_header, std::istream& input)
{
    SQUEEZE_TRACE();

    std::streampos start = target.tellp();

    switch (entry_header.compression.method) {
        using enum compression::CompressionMethod;
    case None:
        input.seekg(0, std::ios_base::end);
        entry_header.content_size = static_cast<uint64_t>(input.tellg());
        input.seekg(0, std::ios_base::beg);
        utils::ioscopy(input, input.tellg(), target, target.tellp(), entry_header.content_size);
        break;
    default:
        throw Exception<Writer>("invalid compression method");
    }

    if (input.fail() || target.fail()) {
        target.clear();
        return input.fail() ? "input reading failure" : "target writing failure";
    }

    return success;
}

Error<Writer> Writer::perform_append_string(EntryHeader& entry_header, const std::string& str)
{
    SQUEEZE_TRACE();

    std::streampos start = target.tellp();

    target.write(str.data(), str.size() + 1);
    if (target.fail()) {
        target.clear();
        return "target writing failure";
    }

    entry_header.content_size = static_cast<uint64_t>(target.tellp() - start);

    return success;
}

}
