#include "squeeze/squeeze.h"

#include <gtest/gtest.h>

#include <sstream>

#include "squeeze/logging.h"
#include "test_tools/generate_mockfs.h"

namespace squeeze::testing {

class SqueezeTest : public ::testing::Test {
protected:
    static void test_mockfs(const tools::MockFileSystem& original, const tools::MockFileSystem& extracted)
    {
    }
};

TEST_F(SqueezeTest, WriteRead)
{
    init_logging();

    std::stringstream content (std::ios_base::binary | std::ios_base::in | std::ios_base::out);
    Squeeze squeeze(content);

    tools::MockFileSystem mockfs = tools::generate_mockfs();
    auto observer =
        [](std::string& path, auto& _)
        {
            std::cerr << '\t' << path << std::endl;
            return 0;
        };
    auto symlink_observer =
        [](std::string& path, std::shared_ptr<tools::MockSymlink> symlink)
        {
            std::cerr << '\t' << path << " -> " << symlink->target << '\n';
            return 0;
        };

    std::cerr << "\033[32m\n" << "Generated a mock filesystem for testing purposes:" << "\033[0m" << '\n';
    mockfs.list_recursively<
        tools::MockRegularFile, tools::MockSymlink, tools::MockDirectory>(
                observer, symlink_observer, observer);

    std::deque<Error<Writer>> write_errors;
    auto append_regular_file = [&squeeze, &write_errors](const std::string& path, std::shared_ptr<tools::MockRegularFile> file)
        {
            write_errors.emplace_back();
            squeeze.will_append<CustomContentEntryInput>(write_errors.back(),
                    std::string(path), CompressionMethod::None, 0, &file->contents, file->get_attributes());
        };
    auto append_directory = [&squeeze, &write_errors](const std::string& path, std::shared_ptr<tools::MockDirectory> file)
        {
            write_errors.emplace_back();
            squeeze.will_append<CustomContentEntryInput>(write_errors.back(),
                    std::string(path), CompressionMethod::None, 0, std::monostate{}, file->get_attributes());
        };
    auto append_symlink = [&squeeze, &write_errors](const std::string& path, std::shared_ptr<tools::MockSymlink> file)
        {
            write_errors.emplace_back();
            squeeze.will_append<CustomContentEntryInput>(write_errors.back(),
                    std::string(path), CompressionMethod::None, 0, file->target, file->get_attributes());
        };

    mockfs.list_recursively<tools::MockRegularFile, tools::MockDirectory, tools::MockSymlink>(
            append_regular_file, append_directory, append_symlink);
    squeeze.write();

    for (const auto& err : write_errors)
        EXPECT_FALSE(err.failed()) << err.report();
}

}
