#include "squeeze/squeeze.h"

#include <gtest/gtest.h>

#include <sstream>

namespace squeeze::testing {

TEST(Squeeze, WriteRead)
{
    std::stringstream target;
    Writer writer(target);
}

}
