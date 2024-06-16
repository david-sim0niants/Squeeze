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
        regular_file = mockfs.make<MockRegularFile>(entry_header.path);
        if (!regular_file)
            return "failed making a regular file in the mock filesystem";
        stream = &regular_file->contents;
        regular_file->set_permissions(entry_header.attributes.permissions);
        break;
    }
    case Directory:
    {
        auto directory = mockfs.make<MockDirectory>(entry_header.path);
        if (!directory)
            return "failed making a directory in the mock filesystem";
        stream = nullptr;
        directory->set_permissions(entry_header.attributes.permissions);
        break;
    }
    case Symlink:
        throw Exception<EntryOutput>("can't create a symlink without a target");
    default:
        throw Exception<EntryOutput>("attempt to extract an entry with an invalid type");
    }
    return success;
}

Error<EntryOutput> MockEntryOutput::init_symlink(
        const EntryHeader& entry_header, const std::string& target)
{
    auto symlink = mockfs.make<MockSymlink>(entry_header.path, std::string(target));
    if (!symlink)
        return "failed making a symlink in the mock filesystem";
    symlink->set_permissions(entry_header.attributes.permissions);
    return success;
}

void MockEntryOutput::deinit() noexcept
{
    regular_file.reset();
}

}
