// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "squeeze/entry_output.h"
#include "squeeze/logging.h"
#include "squeeze/exception.h"
#include "squeeze/utils/fs.h"
#include "squeeze/utils/overloaded.h"

namespace squeeze {

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::FileEntryOutput::"

using Stat = EntryOutput::Stat;

Stat FileEntryOutput::init(EntryHeader&& entry_header, std::ostream *& stream)
{
    switch (entry_header.attributes.get_type()) {
    case EntryType::None:
        return "cannot extract none type entry without a custom output stream";
    case EntryType::RegularFile:
        {
            SQUEEZE_TRACE("'{}' is a regular file", entry_header.path);
            this->final_entry_header = std::move(entry_header);
            return std::visit(utils::Overloaded {
                    [this, &stream](std::ofstream&& f) -> Stat
                    {
                        file = std::move(f);
                        stream = &*file;
                        return success;
                    },
                    [this](StatCode&& stat) -> Stat
                    {
                        return {"failed making a regular file '" + final_entry_header->path + '\'', stat};
                    }
                }, utils::make_regular_file_out(final_entry_header->path));
        }
    case EntryType::Directory:
        {
            SQUEEZE_TRACE("'{}' is a directory", entry_header.path);
            stream = nullptr;
            StatCode sc = utils::make_directory(entry_header.path, entry_header.attributes.get_permissions());
            if (sc.failed())
                return {"failed making directory '" + entry_header.path + '\'', sc};
            else
                return success;
        }
    case EntryType::Symlink:
        throw Exception<EntryOutput>("can't create a symlink without a target");
    default:
        throw Exception<EntryOutput>("attempt to extract an entry with an invalid type");
    }
}

Stat FileEntryOutput::init_symlink(EntryHeader&& entry_header, const std::string &target)
{
    SQUEEZE_TRACE("'{}' is a symlink", entry_header.path);
    StatCode sc = utils::make_symlink(entry_header.path, target, entry_header.attributes.get_permissions());
    if (sc.failed())
        return {"failed creating symlink '" + entry_header.path + " -> " + target + '\'', sc};
    else
        return success;
}

Stat FileEntryOutput::finalize()
{
    SQUEEZE_TRACE();
    if (not final_entry_header)
        return success;

    StatCode sc = utils::set_permissions(final_entry_header->path,
            final_entry_header->attributes.get_permissions());
    if (sc.failed()) {
        SQUEEZE_ERROR("Failed setting file permissions");
        return {"failed setting file permissions", sc};
    }
    return success;
}

void FileEntryOutput::deinit() noexcept
{
    file.reset();
}

Stat CustomStreamEntryOutput::init(EntryHeader&& entry_header, std::ostream *&stream)
{
    stream = &this->stream;
    return success;
}

Stat CustomStreamEntryOutput::init_symlink(EntryHeader&& entry_header, const std::string& target)
{
    stream.write(target.data(), target.size());
    return success;
}

void CustomStreamEntryOutput::deinit() noexcept
{
}

}
