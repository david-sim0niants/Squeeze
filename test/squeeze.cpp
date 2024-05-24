#include "squeeze/squeeze.h"

#include <gtest/gtest.h>

#include <sstream>

namespace squeeze::testing {

class SqueezeTest : ::testing::Test {
protected:
};

TEST_F(SqueezeTest, WriteRead)
{
    std::stringstream content;
    Squeeze squeeze(content);
}

}
