// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "squeeze/entry_input.h"

#include <filesystem>

#include "squeeze/logging.h"
#include "squeeze/utils/fs.h"
#include "squeeze/exception.h"
#include "squeeze/version.h"

namespace squeeze {

using Stat = EntryInput::Stat;

void EntryInput::init_entry_header(EntryHeader& entry_header)
{
    entry_header.version = version;
    entry_header.path = path;
}

void BasicEntryInput::init_entry_header(EntryHeader& entry_header)
{
    EntryInput::init_entry_header(entry_header);
    entry_header.compression = compression;
}

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::FileEntryInput::"

Stat FileEntryInput::init(EntryHeader& entry_header, ContentType& content)
{
    SQUEEZE_TRACE("Opening {}", path);

    auto s = init_entry_header(entry_header);
    if (s.failed())
        return s;

    switch (entry_header.attributes.get_type()) {
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
            return {"failed reading symlink - " + path, Status(ec)};
        content = symlink_target.string();
        break;
    }
    case RegularFile:
        SQUEEZE_TRACE("'{}' is a regular file", path);
        file = std::ifstream(path, std::ios_base::binary | std::ios_base::in);
        if (!*file)
            return "failed opening a file: " + path;
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

Stat FileEntryInput::init_entry_header(EntryHeader& entry_header)
{
    BasicEntryInput::init_entry_header(entry_header);

    StatCode sc;
    std::filesystem::file_status st = std::filesystem::symlink_status(entry_header.path, sc.get());

    if (sc.failed())
        return {stringify("failed getting file status of '", entry_header.path, '\''), sc};

    switch (st.type()) {
        using enum std::filesystem::file_type;
    case regular:
    case directory:
    case symlink:
        EntryType type;
        utils::convert(st.type(), type);
        EntryPermissions perms;
        utils::convert(st.permissions(), perms);
        entry_header.attributes.set_type(type);
        entry_header.attributes.set_permissions(perms);
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

Stat CustomContentEntryInput::init(EntryHeader& entry_header, ContentType& content)
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
