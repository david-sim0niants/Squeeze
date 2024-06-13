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
    future_removes.emplace(std::string(it->second.path), it->first, it->second.get_total_size(), err);
}

void Writer::write()
{
    SQUEEZE_TRACE();
    perform_removes();
    perform_appends();
    SQUEEZE_DEBUG("Stream put pointer at: {}", target.tellp());
}

Error<Writer> Writer::append(std::unique_ptr<EntryInput>&& entry_input)
{
    Error<Writer> err;
    will_append(std::move(entry_input), &err);
    write();
    return err;
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
    SQUEEZE_TRACE("Removing {} entries", future_removes.size());

    target.seekg(0, std::ios_base::end);
    const uint64_t initial_size = target.tellg();
    uint64_t rem_len = 0;
    while (future_removes.size() > 1) {
        std::string path;
        future_removes.top().path.swap(path);
        uint64_t pos = future_removes.top().pos;
        uint64_t len = future_removes.top().len;
        Error<Writer> *error = future_removes.top().error;

        SQUEEZE_TRACE("Removing {}", path);

        uint64_t next_pos = pos;
        while (true) {
            future_removes.pop();
            next_pos = future_removes.top().pos;
            if (pos == next_pos) {
                SQUEEZE_WARN("More than one entry remove with the same position: {} | path: {}", pos, path);
                continue;
            } else {
                break;
            }
        };

        const uint64_t mov_pos = pos + len;
        const uint64_t mov_len = std::min(next_pos, initial_size) - mov_pos;
        utils::iosmove(target, pos - rem_len, mov_pos, mov_len);

        if (target.fail()) {
            SQUEEZE_ERROR("Target output stream failed");
            if (error)
                *error = "failed removing " + path;
            target.clear();
        }

        rem_len += len;
    }
    assert(future_removes.size() == 1 && "The last element in the priority queue of future removes must remain after performing the removes.");
    target.seekp(initial_size - rem_len);
}

void Writer::perform_appends()
{
    SQUEEZE_TRACE("Appending {} entries", future_appends.size());

    for (auto& future_append : future_appends) {
        auto e = perform_append(future_append.entry_input);
        if (future_append.error)
            *future_append.error = e;
    }
    future_appends.clear();
    owned_entry_inputs.clear();
}

Error<Writer> Writer::perform_append(EntryInput& entry_input)
{
    SQUEEZE_TRACE("Appending {}", entry_input.get_path());

    std::streampos initial_pos = target.tellp();

    EntryInput::ContentType content = std::monostate();
    EntryHeader entry_header {};
    auto e = entry_input.init(entry_header, content);
    if (e) {
        SQUEEZE_ERROR("Failed opening an input");
        return {"failed opening an input", e.report()};
    }

    SQUEEZE_DEBUG("entry_header = {}", utils::stringify(entry_header));

    target.seekp(target.tellp() + static_cast<std::streamsize>(entry_header.get_header_size()));

    if (std::holds_alternative<std::istream *>(content)) {
        SQUEEZE_TRACE("Entry content is a stream, appending...");
        e = perform_append_stream(entry_header, *std::get<std::istream *>(content));
    } else if (std::holds_alternative<std::string>(content)) {
        SQUEEZE_TRACE("Entry content is a string, appending...");
        e = perform_append_string(entry_header, std::get<std::string>(content));
    } else {
        SQUEEZE_TRACE("Entry content is empty");
        entry_header.content_size = 0;
    }

    target.seekp(initial_pos);
    if (e) {
        SQUEEZE_ERROR("Failed appending entry");
        return {"failed appending entry", e.report()};
    }

    SQUEEZE_TRACE("Encoding entry header");
    auto ehe = EntryHeader::encode(target, entry_header);
    target.seekp(target.tellp() + static_cast<std::streamsize>(entry_header.content_size));
    if (ehe) {
        SQUEEZE_ERROR("Failed encoding the entry header");
        return {"failed encoding the entry header", ehe.report()};
    }

    return success;
}

Error<Writer> Writer::perform_append_stream(EntryHeader& entry_header, std::istream& input)
{
    SQUEEZE_TRACE();

    std::streampos start = target.tellp();

    switch (entry_header.compression_method) {
        using enum CompressionMethod;
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
