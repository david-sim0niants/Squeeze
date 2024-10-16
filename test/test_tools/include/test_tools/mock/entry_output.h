// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include "squeeze/entry_output.h"
#include "fs.h"

namespace squeeze::test_tools::mock {

class EntryOutput final : public squeeze::EntryOutput {
public:
    explicit EntryOutput(FileSystem& fs) : fs(fs) {}

    Stat init(EntryHeader&& entry_header, std::ostream *& stream) override;
    Stat init_symlink(EntryHeader&& entry_header, const std::string& target) override;
    Stat finalize() override;
    void deinit() noexcept override;

private:
    FileSystem& fs;
    std::shared_ptr<AbstractFile> file = nullptr;
    EntryPermissions permissions {};
};

}
