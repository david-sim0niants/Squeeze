#include "squeeze/entry_header.h"

#include <sstream>

#include <gtest/gtest.h>

#include "squeeze/version.h"

#include "test_tools/random.h"

namespace squeeze::testing {

TEST(EntryHeader, EncodeDecode)
{
    tools::Random<int> prng(1234);

    EntryHeader original_entry_header = {
        .content_size = static_cast<uint64_t>(prng(0, std::numeric_limits<int>::max())),
        .major_minor_version = {version.major, version.minor},
        .compression = {
            .method = compression::CompressionMethod::None,
            .level = 0,
        },
        .attributes = {
            .type = (EntryType)prng(0, 3),
            .permissions = (EntryPermissions)prng(0,
                    static_cast<std::underlying_type_t<EntryPermissions>>(EntryPermissions::All)),
        },
        .path = tools::generate_random_string(prng, prng(32, 64)),
    }, restored_entry_header;

    std::stringstream stream;

    EntryHeader::encode(stream, original_entry_header);
    std::cerr << "\033[32m" "original_entry_header=" << utils::stringify(original_entry_header) << "\033[0m\n";

    EntryHeader::decode(stream, restored_entry_header);
    std::cerr << "\033[32m" "restored_entry_header=" << utils::stringify(restored_entry_header) << "\033[0m\n";

    EXPECT_EQ(original_entry_header.content_size,
              restored_entry_header.content_size);

    EXPECT_EQ(original_entry_header.major_minor_version.major,
              restored_entry_header.major_minor_version.major);

    EXPECT_EQ(original_entry_header.major_minor_version.minor,
              restored_entry_header.major_minor_version.minor);

    EXPECT_EQ(original_entry_header.compression.method,
              restored_entry_header.compression.method);

    EXPECT_EQ(original_entry_header.compression.level,
              restored_entry_header.compression.level);

    EXPECT_EQ(original_entry_header.attributes.type,
              restored_entry_header.attributes.type);

    EXPECT_EQ(original_entry_header.attributes.permissions,
              restored_entry_header.attributes.permissions);

    EXPECT_EQ(original_entry_header.path.size(),
              restored_entry_header.path.size());

    EXPECT_EQ(original_entry_header.path,
              restored_entry_header.path);
}

}
