// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include "squeeze/entry_input.h"
#include "fs.h"

namespace squeeze::test_tools::mock {

class EntryInput final : public BasicEntryInput {
public:
    template<typename File>
    EntryInput(std::string&& path, const CompressionParams& compression,
            std::shared_ptr<File> file)
        requires(std::is_base_of_v<AbstractFile, File>)
        : BasicEntryInput(std::move(path), compression), file(file)
    {}

    Stat init(EntryHeader& entry_header, ContentType& content) override;
    void deinit() noexcept override;

    void init_entry_header(EntryHeader& entry_header);

private:
    std::variant<
        std::shared_ptr<RegularFile>,
        std::shared_ptr<Directory>,
        std::shared_ptr<Symlink>
    > file;
};

}
