#pragma once

#include "squeeze/entry_output.h"
#include "mockfs.h"

namespace squeeze::testing::tools {

class MockEntryOutput final : public EntryOutput {
public:
    explicit MockEntryOutput(tools::MockFileSystem& mockfs) : mockfs(mockfs) {}

    Error<EntryOutput> init(const EntryHeader& entry_header, std::ostream *& stream) override;
    Error<EntryOutput> init_symlink(const EntryHeader& entry_header, const std::string& target) override;
    Error<EntryOutput> finalize() override;
    void deinit() noexcept override;

private:
    tools::MockFileSystem& mockfs;
    std::shared_ptr<tools::MockAbstractFile> file = nullptr;
    EntryPermissions permissions {};
};

}
