// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <cstring>
#include <functional>
#include <mutex>
#include <filesystem>
#include <string>
#include <string_view>

namespace squeeze::test_common {

namespace fs = std::filesystem;

class TestData {
public:
    inline static std::string_view get_data_seed()
    {
        instance.prep_data_seed();
        return instance.data_seed;
    }

    static constexpr char default_data_seed[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz ";
    inline static const fs::path data_seed_default_path =
        fs::absolute(fs::path(__FILE__).parent_path()) / "data/data_seed.txt";
    static constexpr char data_seed_path_env_name[] = "SQUEEZE_TEST_DATA_SEED_PATH";

private:
    TestData() = default;

    static TestData instance;

    inline void prep_data_seed()
    {
        std::call_once(data_seed_once_flag, std::mem_fn(&TestData::load_data_seed), this);
    }

    static fs::path get_data_seed_path();
    void load_data_seed();

    std::string data_seed = default_data_seed;
    std::once_flag data_seed_once_flag;
};

inline TestData TestData::instance;

}
