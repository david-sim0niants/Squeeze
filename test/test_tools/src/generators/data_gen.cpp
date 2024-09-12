#include "test_tools/generators/data_gen.h"

namespace squeeze::test_tools::generators {

namespace {

template<std::input_iterator InIt>
void apply_noise_on_str(PRNG prng, unsigned char noise_probability, InIt char_it, InIt char_it_end)
{
    for (; char_it != char_it_end; ++char_it) {
        char& c = *char_it;
        c = prng.gen<unsigned char>(0, 255) <= noise_probability ? char(prng(0, 255)) : c;
    }
}

template<bool apply_noise>
std::vector<char> gen_data(const PRNG& prng, const std::string_view data_seed,
        const std::size_t size, unsigned char noise_probability)
{
    std::vector<char> data; data.reserve(size);
    if (0 == size || data_seed.empty())
        return data;

    const std::size_t size_limit = std::min(size, data_seed.size());

    std::size_t curr_size = size;
    while (curr_size) {
        const std::size_t max_substr_size = std::min(size_limit, curr_size);
        const std::size_t min_substr_size = std::max(max_substr_size / 2, std::size_t(1));
        const std::size_t substr_size = prng(min_substr_size, max_substr_size);
        const std::size_t substr_pos = prng(std::size_t(0), data_seed.size() - substr_size);
        const std::string_view substr = data_seed.substr(substr_pos, substr_size); 
        data.insert(data.end(), substr.data(), substr.end());
        if constexpr (apply_noise)
            apply_noise_on_str(prng, noise_probability, data.end() - substr_size, data.end());
        curr_size -= substr_size;
    }

    return data;
}

}

static constexpr std::size_t min_reasonable_size = 64;
static constexpr std::size_t max_reasonable_size = 64 << 20;

std::vector<char> gen_data(const PRNG& prng, const std::string_view data_seed)
{
    const std::size_t size = prng(std::min(min_reasonable_size, data_seed.size() / 4),
                                  std::min(max_reasonable_size, data_seed.size()));
    return gen_data<false>(prng, data_seed, size, 0);
}

std::vector<char> gen_data(const PRNG& prng, const std::string_view data_seed, unsigned char noise_probability)
{
    const std::size_t size = prng(std::min(min_reasonable_size, data_seed.size() / 4),
                                  std::min(max_reasonable_size, data_seed.size()));
    return gen_data<true>(prng, data_seed, size, noise_probability);
}

std::vector<char> gen_data(const PRNG& prng, const std::string_view data_seed, std::size_t size)
{
    return gen_data<false>(prng, data_seed, size, 0);
}

std::vector<char> gen_data(const PRNG& prng, const std::string_view data_seed,
        std::size_t size, unsigned char noise_probability)
{
    return gen_data<true>(prng, data_seed, size, noise_probability);
}

std::string gen_alphanumeric_string(std::size_t size, const PRNG& prng)
{
    std::string str;
    str.resize(size);

    constexpr char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
    constexpr size_t alphanum_max_idx = sizeof(alphanum) / sizeof(alphanum[0]) - 2;

    for (size_t i = 0; i < size; ++i)
        str[i] = alphanum[prng(std::size_t(0), alphanum_max_idx)];
    return str;
}

}
