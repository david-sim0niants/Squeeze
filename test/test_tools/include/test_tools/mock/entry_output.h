#pragma once

#include "squeeze/entry_output.h"
#include "fs.h"

namespace squeeze::test_tools::mock {

class EntryOutput final : public squeeze::EntryOutput {
public:
    explicit EntryOutput(FileSystem& fs) : fs(fs) {}

    Error<EntryOutput> init(const EntryHeader& entry_header, std::ostream *& stream) override;
    Error<EntryOutput> init_symlink(const EntryHeader& entry_header, const std::string& target) override;
    Error<EntryOutput> finalize() override;
    void deinit() noexcept override;

private:
    FileSystem& fs;
    std::shared_ptr<AbstractFile> file = nullptr;
    EntryPermissions permissions {};
};

}
