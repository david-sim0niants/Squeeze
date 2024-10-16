// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "test_data.h"

#include <fstream>
#include <iostream>

namespace squeeze::test_common {

fs::path TestData::get_data_seed_path()
{
    char *data_seed_path_str = std::getenv(data_seed_path_env_name);
    if (!data_seed_path_str)
        return data_seed_default_path;
    constexpr std::size_t maxlen = 256;
    char *data_seed_path_str_end = static_cast<char *>(std::memchr(data_seed_path_str, '\0', maxlen));
    fs::path data_seed_path(data_seed_path_str, data_seed_path_str_end);
    if (fs::status(data_seed_path).type() == fs::file_type::regular)
        return data_seed_path;
    else
        return data_seed_default_path.c_str();
}

void TestData::load_data_seed()
{
    static constexpr std::size_t max_file_size = 2 << 20;
    std::ifstream file(get_data_seed_path(), std::ios_base::binary);
    if (!file) {
        file = std::ifstream(data_seed_default_path, std::ios_base::binary);
        if (!file)
            return;
    }

    file.seekg(0, std::ios::end);
    data_seed.resize(std::min(static_cast<std::size_t>(file.tellg()), max_file_size));
    file.seekg(0, std::ios::beg);

    file.read(data_seed.data(), data_seed.size());

    data_seed.resize(file.gcount());
}

}
