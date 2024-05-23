#include "squeeze/entry_input.h"

#include <filesystem>

#include "squeeze/utils/fs.h"
#include "squeeze/exception.h"


namespace squeeze {

void EntryInput::init_entry_header(EntryHeader& entry_header)
{
    entry_header.path_len = path.size();
    entry_header.path = path;
}


void BasicEntryInput::init_entry_header(EntryHeader& entry_header)
{
    EntryInput::init_entry_header(entry_header);
    entry_header.compression_method = compression_method;
    entry_header.compression_level = compression_level;
}


Error<EntryInput> FileEntryInput::init(std::istream *& stream, EntryHeader& entry_header)
{
    auto e = init_entry_header(entry_header);
    if (e)
        return e;

    *file = std::ifstream(path, std::ios_base::binary | std::ios_base::in);
    if (!*file)
        return {"failed opening a file", ErrorCode::from_current_errno().report()};

    stream = &*file;
    entry_header.content_size = 0;
    return success;
}

void FileEntryInput::deinit()
{
    file.reset();
}

Error<EntryInput> FileEntryInput::init_entry_header(EntryHeader& entry_header)
{
    BasicEntryInput::init_entry_header(entry_header);

    std::filesystem::path path(this->path);
    std::filesystem::file_status st = std::filesystem::status(path);
    switch (st.type()) {
        using enum std::filesystem::file_type;
    case regular:
    case directory:
    case symlink:
        EntryType type;
        utils::convert(st.type(), type);
        EntryPermissions perms;
        utils::convert(st.permissions(), perms);
        entry_header.attributes.type = type;
        entry_header.attributes.permissions = perms;
        break;
    case not_found:
        return "no such file or directory";
    case block:
        return "block file type not supported";
    case character:
        return "character file type not supported";
    case fifo:
        return "fifo file type not supported";
    case socket:
        return "socket file type not supported";
    case unknown:
        return "unknown file type";
    default:
        throw BaseException("unexpected file type");
    }

    return success;
}


Error<EntryInput> CustomStreamEntryInput::init(std::istream *& stream, EntryHeader& entry_header)
{
    init_entry_header(entry_header);
    stream = &this->stream;
    return success;
}

void CustomStreamEntryInput::deinit()
{
}

void CustomStreamEntryInput::init_entry_header(EntryHeader& entry_header)
{
    BasicEntryInput::init_entry_header(entry_header);
    entry_header.attributes = entry_attributes;
}

}
