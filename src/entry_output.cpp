#include "squeeze/entry_output.h"
#include "squeeze/logging.h"
#include "squeeze/exception.h"
#include "squeeze/utils/fs.h"

namespace squeeze {

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::FileEntryOutput::"

Error<EntryOutput> FileEntryOutput::init(const EntryHeader& entry_header, std::ostream *& stream)
{
    switch (entry_header.attributes.type) {
    case EntryType::None:
        return "cannot extract none type entry without a custom output stream";
    case EntryType::RegularFile:
        {
            SQUEEZE_TRACE("'{}' is a regular file", entry_header.path);
            auto result = utils::make_regular_file_out(
                    entry_header.path, entry_header.attributes.permissions);
            if (auto *f = std::get_if<std::ofstream>(&result)) {
                file = std::move(*f);
                stream = &*file;
                return success;
            } else {
                return {"failed making regular file '" + entry_header.path + '\'', get<ErrorCode>(result).report()};
            }
        }
    case EntryType::Directory:
        {
            SQUEEZE_TRACE("'{}' is a directory", entry_header.path);
            stream = nullptr;
            auto e = utils::make_directory(entry_header.path, entry_header.attributes.permissions);
            if (e.failed())
                return {"failed making directory '" + entry_header.path + '\'', e.report()};
            else
                return success;
        }
    case EntryType::Symlink:
        throw Exception<EntryOutput>("can't create a symlink without a target");
    default:
        throw Exception<EntryOutput>("attempt to extract an entry with an invalid type");
    }
}

Error<EntryOutput> FileEntryOutput::init_symlink(
        const EntryHeader &entry_header, const std::string &target)
{
    SQUEEZE_TRACE("'{}' is a symlink", entry_header.path);
    ErrorCode ec = utils::make_symlink(entry_header.path, target, entry_header.attributes.permissions);
    if (ec)
        return {"failed creating symlink '" + entry_header.path + " -> " + target + '\'', ec.report()};
    else
        return success;
}

void FileEntryOutput::deinit() noexcept
{
    file.reset();
}


Error<EntryOutput> CustomStreamEntryOutput::init(
        const EntryHeader &entry_header, std::ostream *&stream)
{
    stream = &this->stream;
    return success;
}

Error<EntryOutput> CustomStreamEntryOutput::init_symlink(
        const EntryHeader &entry_header, const std::string& target)
{
    stream << target;
    return success;
}

void CustomStreamEntryOutput::deinit() noexcept
{
}

}
