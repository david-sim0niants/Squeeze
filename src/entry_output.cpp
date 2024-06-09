#include "squeeze/entry_output.h"
#include "squeeze/exception.h"
#include "squeeze/utils/fs.h"

namespace squeeze {

Error<EntryOutput> FileEntryOutput::init(const EntryHeader& entry_header, std::ostream *& stream)
{
    switch (entry_header.attributes.type) {
    case EntryType::None:
        return "cannot extract none type entry without a custom output stream";
    case EntryType::RegularFile:
        {
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
    ErrorCode ec = utils::make_symlink(entry_header.path, target, entry_header.attributes.permissions);
    if (ec)
        return {"failed initializing symlink '" + entry_header.path + " -> " + target + '\'', ec.report()};
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
