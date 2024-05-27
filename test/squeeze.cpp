#include "squeeze/squeeze.h"

#include <gtest/gtest.h>

#include <sstream>

#include "test_tools/generate_mockfs.h"

namespace squeeze::testing {

class SqueezeTest : public ::testing::Test {
protected:
    static void test_mockfs(const tools::MockFileSystem& a, const tools::MockFileSystem& b)
    {
    }
};

TEST_F(SqueezeTest, WriteRead)
{
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
}

}
