#pragma once

#include "test_common/test_data.h"

#include "test_tools/generators/data_gen.h"
#include "test_tools/generators/prng.h"
#include "test_tools/generators/test_gen.h"
#include "test_tools/printing.h"

namespace squeeze::test_common {

class TestOnDataInput {
public:
    virtual std::vector<char> get_data() const = 0;
    virtual void print_to(std::ostream *os) const = 0;
};

class TestOnDataInputRandom : public TestOnDataInput {
public:
    explicit TestOnDataInputRandom(int prng_seed, unsigned char noise_probability = 0)
        : prng_seed(prng_seed), noise_probability(noise_probability)
    {
    }

    std::vector<char> get_data() const override
    {
        if (noise_probability != 0)
            return gen_data(test_tools::generators::PRNG(prng_seed), TestData::get_data_seed(),
                            noise_probability);
        else
            return gen_data(test_tools::generators::PRNG(prng_seed), TestData::get_data_seed());
    }

    void print_to(std::ostream *os) const override
    {
        *os << "Pseudo-random test input: {prng_seed=" << prng_seed
            << ", noise_probability=" << static_cast<unsigned int>(noise_probability)
            << "/255} which generates the following data: ";
        std::vector<char> data(get_data());
        test_tools::print_data(data.data(), data.size(), *os, 128);
    }

private:
    int prng_seed;
    unsigned char noise_probability = 0;
};

class TestOnDataInputCustom : public TestOnDataInput {
public:
    explicit TestOnDataInputCustom(std::vector<char>&& data) : data(std::move(data))
    {
    }

    std::vector<char> get_data() const override
    {
        return data;
    }

    void print_to(std::ostream *os) const override
    {
        *os << "Custom data test input: ";
        test_tools::print_data(data.data(), data.size(), *os, 128);
    }

private:
    std::vector<char> data;
};

inline void PrintTo(const TestOnDataInput *input, std::ostream *os)
{
    input->print_to(os);
}

std::vector<TestOnDataInputRandom> gen_random_inputs(std::size_t nr_inputs, int start_seed,
        unsigned char noise_probability = 0);

inline std::vector<TestOnDataInputCustom> get_default_custom_inputs()
{
    return {TestOnDataInputCustom(std::vector(TestData::get_data_seed().begin(),
                                  TestData::get_data_seed().end()))};
}

inline auto make_generated_test_on_data_inputs(
        std::size_t tot_nr_inputs, int start_seed, unsigned char noise_probability,
        std::vector<TestOnDataInputCustom> custom_inputs = get_default_custom_inputs())
{
    using test_tools::generators::make_generated_test_inputs;
    const std::size_t nr_random_inputs = tot_nr_inputs - std::min(tot_nr_inputs, custom_inputs.size());
    return make_generated_test_inputs<TestOnDataInput>(
            gen_random_inputs(nr_random_inputs, start_seed, noise_probability), std::move(custom_inputs));
}

inline auto make_generated_test_on_data_inputs(std::size_t tot_nr_inputs, int start_seed,
        std::vector<TestOnDataInputCustom> custom_inputs = get_default_custom_inputs())
{
    using test_tools::generators::make_generated_test_inputs;
    const std::size_t nr_random_inputs = tot_nr_inputs - std::min(tot_nr_inputs, custom_inputs.size());
    return make_generated_test_inputs<TestOnDataInput>(
            gen_random_inputs(nr_random_inputs, start_seed), std::move(custom_inputs));
}

}
