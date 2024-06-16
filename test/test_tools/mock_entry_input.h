#pragma once

#include "squeeze/entry_input.h"
#include "mockfs.h"

namespace squeeze::testing::tools {

class MockEntryInput final : public BasicEntryInput {
public:
    template<typename MockFile>
    MockEntryInput(std::string&& path, CompressionMethod compression_method, int compression_level,
            std::shared_ptr<MockFile> file)
        requires(std::is_base_of_v<MockAbstractFile, MockFile>)
        : BasicEntryInput(std::move(path), compression_method, compression_level), file(file)
    {}

    Error<EntryInput> init(EntryHeader& entry_header, ContentType& content) override;
    void deinit() noexcept override;

    void init_entry_header(EntryHeader& entry_header);

private:
    std::variant<
        std::shared_ptr<MockRegularFile>,
        std::shared_ptr<MockDirectory>,
        std::shared_ptr<MockSymlink>
    > file;
};

}
