#include "squeeze/entry_input.h"

#include <filesystem>

#include "squeeze/logging.h"
#include "squeeze/utils/fs.h"
#include "squeeze/exception.h"
#include "squeeze/version.h"

namespace squeeze {

void EntryInput::init_entry_header(EntryHeader& entry_header)
{
    entry_header.major_minor_version = {version.major, version.minor};
    entry_header.path = path;
}

void BasicEntryInput::init_entry_header(EntryHeader& entry_header)
{
    EntryInput::init_entry_header(entry_header);
    entry_header.compression = compression;
}

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::FileEntryInput::"

Error<EntryInput> FileEntryInput::init(EntryHeader& entry_header, ContentType& content)
{
    SQUEEZE_TRACE("Opening {}", path);

    auto e = init_entry_header(entry_header);
    if (e)
        return e;

    switch (entry_header.attributes.type) {
        using enum EntryType;
    case Directory:
        SQUEEZE_TRACE("'{}' is a directory", path);
        content = std::monostate();
        break;
    case Symlink:
    {
        SQUEEZE_TRACE("'{}' is a symlink", path);
        std::error_code ec;
        auto symlink_target = std::filesystem::read_symlink(entry_header.path, ec);
        if (ec)
            return {"failed reading symlink", ErrorCode(ec).report()};
        content = symlink_target.string();
        break;
    }
    case RegularFile:
        SQUEEZE_TRACE("'{}' is a regular file", path);
        file = std::ifstream(path, std::ios_base::binary | std::ios_base::in);
        if (!*file)
            return {"failed opening a file", ErrorCode::from_current_errno().report()};
        content = &*file;
        break;
    default:
        throw Exception<EntryInput>("unexpected file type");
    }

    entry_header.content_size = 0;
    return success;
}

void FileEntryInput::deinit() noexcept
{
    file.reset();
}

Error<EntryInput> FileEntryInput::init_entry_header(EntryHeader& entry_header)
{
    BasicEntryInput::init_entry_header(entry_header);

    ErrorCode ec;
    std::filesystem::file_status st = std::filesystem::symlink_status(entry_header.path, ec.get());

    if (ec)
        return {"failed getting file status of '" + entry_header.path + '\'', ec.report()};

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
        return success;
    case not_found:
        return "no such file or directory - " + entry_header.path;
    case block:
        return "block file type not supported - " + entry_header.path;
    case character:
        return "character file type not supported - " + entry_header.path;
    case fifo:
        return "fifo file type not supported - " + entry_header.path;
    case socket:
        return "socket file type not supported - " + entry_header.path;
    case unknown:
        return "unknown file type - " + entry_header.path;
    default:
        throw Exception<EntryInput>("unexpected file type");
    }
}

Error<EntryInput> CustomContentEntryInput::init(EntryHeader& entry_header, ContentType& content)
{
    init_entry_header(entry_header);
    content = this->content;
    return success;
}

void CustomContentEntryInput::deinit() noexcept
{
}

void CustomContentEntryInput::init_entry_header(EntryHeader& entry_header)
{
    BasicEntryInput::init_entry_header(entry_header);
    entry_header.attributes = entry_attributes;
}

}
