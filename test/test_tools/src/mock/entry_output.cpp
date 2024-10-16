// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "test_tools/mock/entry_output.h"

#include "squeeze/exception.h"

namespace squeeze::test_tools::mock {

using Stat = EntryOutput::Stat;

Stat EntryOutput::init(EntryHeader&& entry_header, std::ostream *& stream)
{
    switch (entry_header.attributes.get_type()) {
        using enum EntryType;
    case None:
        return "none type not supported";
    case RegularFile:
    {
        auto regular_file = fs.make<mock::RegularFile>(entry_header.path);
        this->file = regular_file;
        if (!regular_file)
            return "failed making a regular file in the mock filesystem";
        stream = &regular_file->contents;
        break;
    }
    case Directory:
    {
        auto directory = fs.make<mock::Directory>(entry_header.path);
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
    permissions = entry_header.attributes.get_permissions();
    return success;
}

Stat EntryOutput::init_symlink(EntryHeader&& entry_header, const std::string& target)
{
    auto symlink = fs.make<Symlink>(entry_header.path, std::string(target));
    this->file = symlink;
    if (!symlink)
        return "failed making a symlink in the mock filesystem";
    permissions = entry_header.attributes.get_permissions();
    return success;
}

Stat EntryOutput::finalize()
{
    file->set_permissions(permissions);
    return success;
}

void EntryOutput::deinit() noexcept
{
    file.reset();
}

}
