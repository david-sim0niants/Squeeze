#include "test_on_data_input.h"

namespace squeeze::test_common {

std::vector<TestOnDataInputRandom> gen_random_inputs(std::size_t nr_inputs, int start_seed,
        unsigned char noise_probability)
{
    std::vector<TestOnDataInputRandom> random_inputs;
    random_inputs.reserve(nr_inputs);
    for (std::size_t i = 0; i < nr_inputs; ++i)
        random_inputs.emplace_back(i + start_seed, noise_probability);
    return random_inputs;
}

}
