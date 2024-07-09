#include "mock_entry_output.h"

#include "squeeze/exception.h"

namespace squeeze::testing::tools {

Error<EntryOutput> MockEntryOutput::init(
        const EntryHeader& entry_header, std::ostream *& stream)
{
    switch (entry_header.attributes.type) {
        using enum EntryType;
    case None:
        return "none type not supported";
    case RegularFile:
    {
        auto regular_file = mockfs.make<MockRegularFile>(entry_header.path);
        this->file = regular_file;
        if (!regular_file)
            return "failed making a regular file in the mock filesystem";
        stream = &regular_file->contents;
        break;
    }
    case Directory:
    {
        auto directory = mockfs.make<MockDirectory>(entry_header.path);
        this->file = directory;
        if (!directory)
            return "failed making a directory in the mock filesystem";
        stream = nullptr;
        break;
    }
    case Symlink:
        throw Exception<EntryOutput>("can't create a symlink without a target");
    default:
        throw Exception<EntryOutput>("attempt to extract an entry with an invalid type");
    }
    permissions = entry_header.attributes.permissions;
    return success;
}

Error<EntryOutput> MockEntryOutput::init_symlink(
        const EntryHeader& entry_header, const std::string& target)
{
    auto symlink = mockfs.make<MockSymlink>(entry_header.path, std::string(target));
    this->file = symlink;
    if (!symlink)
        return "failed making a symlink in the mock filesystem";
    permissions = entry_header.attributes.permissions;
    return success;
}

Error<EntryOutput> MockEntryOutput::finalize()
{
    file->set_permissions(permissions);
    return success;
}

void MockEntryOutput::deinit() noexcept
{
    file.reset();
}

}
