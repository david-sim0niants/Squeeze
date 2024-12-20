// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "squeeze/entry_header.h"

#include <sstream>

#include <gtest/gtest.h>

#include "squeeze/version.h"

#include "test_tools/generators/data_gen.h"

namespace squeeze::testing {

TEST(EntryHeader, EncodeDecode)
{
    test_tools::generators::PRNG prng(1234);
    const auto permissions_min = std::underlying_type_t<EntryPermissions>{};
    const auto permissions_max =
            static_cast<std::underlying_type_t<EntryPermissions>>(EntryPermissions::All);

    EntryHeader original_entry_header = {
        .version = version,
        .content_size = static_cast<uint64_t>(prng(0, std::numeric_limits<int>::max())),
        .compression = {
            .method = compression::CompressionMethod::None,
            .level = 0,
        },
        .attributes = {
            EntryType(prng(0, 3)),
            EntryPermissions(prng(permissions_min, permissions_max)),
        },
        .path = test_tools::generators::gen_alphanumeric_string(prng(32, 64), prng),
    }, restored_entry_header;

    std::stringstream stream;

    EntryHeader::encode(stream, original_entry_header);
    EXPECT_EQ(stream.tellp(), original_entry_header.get_encoded_header_size());

    EntryHeader::decode(stream, restored_entry_header);
    EXPECT_EQ(original_entry_header.get_encoded_header_size(), restored_entry_header.get_encoded_header_size());

    EXPECT_EQ(original_entry_header.content_size,
              restored_entry_header.content_size);

    EXPECT_EQ(original_entry_header.version.data,
              restored_entry_header.version.data);

    EXPECT_EQ(original_entry_header.compression.method,
              restored_entry_header.compression.method);

    EXPECT_EQ(original_entry_header.compression.level,
              restored_entry_header.compression.level);

    EXPECT_EQ(original_entry_header.attributes.get_type(),
              restored_entry_header.attributes.get_type());

    EXPECT_EQ(original_entry_header.attributes.get_permissions(),
              restored_entry_header.attributes.get_permissions());

    EXPECT_EQ(original_entry_header.path.size(),
              restored_entry_header.path.size());

    EXPECT_EQ(original_entry_header.path,
              restored_entry_header.path);
}

}
