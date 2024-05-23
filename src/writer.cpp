#include "squeeze/writer.h"

#include "squeeze/exception.h"
#include "squeeze/utils/io.h"
#include "squeeze/utils/fs.h"

#include <cassert>

namespace squeeze {

struct Writer::FutureAppend {
    EntryInput& entry_input;
    Error<Writer> *error;

    FutureAppend(EntryInput& entry_input, Error<Writer> *error)
        : entry_input(entry_input), error(error)
    {}
};

struct Writer::FutureRemove {
    mutable std::string path;
    uint64_t pos, len;
    Error<Writer> *error;

    FutureRemove(std::string&& path, uint64_t pos, uint64_t len, Error<Writer> *error)
        : path(std::move(path)), pos(pos), len(len), error(error)
    {}
};

bool Writer::FutureRemoveCompare::operator()(const FutureRemove& a, const FutureRemove& b)
{
    return a.pos < b.pos;
}


Writer::Writer(std::iostream& target) : target(target), reader(target)
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
    future_appends.emplace_back(entry_input, err);
}

void Writer::will_remove(const ReaderIterator& it, Error<Writer> *err)
{
    future_removes.emplace(std::string(it->second.path), it->first, it->second.get_total_size(), err);
}

void Writer::write()
{
    perform_removes();
    perform_appends();
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
    uint64_t rem_len = 0;
    while (future_removes.size() > 1) {
        std::string path;
        future_removes.top().path.swap(path);
        uint64_t pos = future_removes.top().pos;
        uint64_t len = future_removes.top().len;
        Error<Writer> *error = future_removes.top().error;
        future_removes.pop();

        uint64_t next_pos = future_removes.top().pos;

        const uint64_t mov_pos = pos + len;
        const uint64_t mov_len = next_pos - pos;
        utils::iosmove(target, mov_pos - rem_len, mov_pos, mov_len);

        if (target.fail()) {
            if (error)
                *error = {"failed removing " + path, ErrorCode::from_current_errno().report()};
        }

        rem_len += len;
    }
    assert(future_removes.size() == 1);
}

void Writer::perform_appends()
{
    target.seekp(0, std::ios_base::end);
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
    std::istream *stream;
    EntryHeader entry_header {};
    auto e = entry_input.init(stream, entry_header);
    if (e)
        return {"failed opening an input", e.report()};

    std::streampos initial_target_size = target.tellp();
    target.seekp(target.tellp() + static_cast<std::streamsize>(entry_header.get_header_size()));

    switch (entry_header.compression_method) {
        using enum CompressionMethod;
    case None:
        stream->seekg(0, std::ios_base::end);
        entry_header.content_size = static_cast<uint64_t>(stream->tellg());
        stream->seekg(0);
        utils::ioscopy(*stream, stream->tellg(), target, target.tellg(), entry_header.content_size);
        break;
    default:
        throw Exception<Writer>("invalid compression method");
    }

    if (stream->fail() || target.fail())
        return "I/O failure";

    target.seekp(initial_target_size);
    auto ehe = EntryHeader::encode(target, entry_header);
    if (ehe)
        return {"failed encoding the entry header", ehe.report()};

    return success;
}

}
