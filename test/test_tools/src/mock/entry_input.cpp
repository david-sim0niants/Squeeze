#include "test_tools/mock/entry_input.h"

#include "squeeze/utils/overloaded.h"

namespace squeeze::test_tools::mock {

Error<EntryInput> EntryInput::init(EntryHeader &entry_header, ContentType &content)
{
    init_entry_header(entry_header);
    content = std::visit( utils::Overloaded {
            [](std::shared_ptr<RegularFile> file) -> ContentType
                        { file->contents.seekg(0, std::ios_base::beg); return &file->contents;  },
            [](std::shared_ptr<Directory> file)   -> ContentType { return std::monostate(); },
            [](std::shared_ptr<Symlink> file)     -> ContentType { return file->target;     },
        }, file);
    return success;
}

void EntryInput::deinit() noexcept
{
}

void EntryInput::init_entry_header(EntryHeader &entry_header)
{
    BasicEntryInput::init_entry_header(entry_header);
    entry_header.attributes = std::visit(
        [](auto file)
        {
            return file->get_attributes();
        }, file);
}

}
