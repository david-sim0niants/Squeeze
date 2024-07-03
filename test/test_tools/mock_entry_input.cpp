#include "mock_entry_input.h"

#include "squeeze/utils/overloaded.h"

namespace squeeze::testing::tools {

Error<EntryInput> MockEntryInput::init(EntryHeader &entry_header, ContentType &content)
{
    init_entry_header(entry_header);
    content = std::visit( utils::Overloaded {
            [](std::shared_ptr<MockRegularFile> file) -> ContentType
                        { file->contents.seekg(0, std::ios_base::beg); return &file->contents;  },
            [](std::shared_ptr<MockDirectory> file)   -> ContentType { return std::monostate(); },
            [](std::shared_ptr<MockSymlink> file)     -> ContentType { return file->target;     },
        }, file);
    return success;
}

void MockEntryInput::deinit() noexcept
{
}

void MockEntryInput::init_entry_header(EntryHeader &entry_header)
{
    BasicEntryInput::init_entry_header(entry_header);
    entry_header.attributes = std::visit(
        [](auto file)
        {
            return file->get_attributes();
        }, file);
}

}
