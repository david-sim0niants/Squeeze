#include "random.h"


namespace squeeze::testing::tools {

std::string generate_random_string(const Random<int>& prng, size_t size)
{
    std::string str;
    str.resize(size);

    constexpr char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
    constexpr size_t alphanum_max_idx = sizeof(alphanum) / sizeof(alphanum[0]) - 2;

    for (size_t i = 0; i < size; ++i)
        str[i] = alphanum[prng(0, alphanum_max_idx)];
    return str;
}


}
