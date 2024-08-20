#include "generate_mock_contents.h"

namespace squeeze::testing::tools {

void generate_mock_contents(const MockContentsGeneratorParams& params,
        const Random<int>& prng, const std::string_view seed, std::ostream& contents)
{
    int nr_reps = (params.flags & MockContentsGeneratorParams::RandomizedRepCount) ?
            prng(params.min_nr_reps, params.max_nr_reps) : params.min_nr_reps;
    size_t curr_size = 0;
    while (nr_reps-- && curr_size < params.size_limit) {
        size_t pos = 0, len = seed.size();
        if (params.flags & MockContentsGeneratorParams::RandomizedRange) {
            pos = prng(0, len - 1);
            len = prng(0, len - 1 - pos);
        }
        len = std::min(len, params.size_limit - curr_size);

        std::string data {seed.substr(pos, len)};
        if (params.flags & MockContentsGeneratorParams::ApplyNoise) {
            for (char& c : data) {
                c = static_cast<unsigned char>(prng(0, 256)) <= params.noise_probability ?
                        static_cast<char>(prng(0, 256)) : c;
            }
        }
        contents.write(data.data(), data.size());
        curr_size += len;
    }
}

}
