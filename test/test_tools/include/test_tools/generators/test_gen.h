#pragma once

#include <vector>

namespace squeeze::test_tools::generators {

template<typename TestInput, typename TestInputRandom, typename TestInputCustom>
    requires std::is_base_of_v<TestInput, TestInputRandom> &&
             std::is_base_of_v<TestInput, TestInputCustom>
class GeneratedTestInputs {
public:
    GeneratedTestInputs(std::vector<TestInputRandom>&& random_inputs,
                        std::vector<TestInputCustom>&& custom_inputs)
        :   random_inputs(std::move(random_inputs)),
            custom_inputs(std::move(custom_inputs))
    {
    }

    std::vector<const TestInput *> collect() const
    {
        std::vector<const TestInput *> inputs;
        inputs.reserve(random_inputs.size() + custom_inputs.size());
        for (auto& random_input : random_inputs)
            inputs.emplace_back(&random_input);
        for (auto& custom_input : custom_inputs)
            inputs.emplace_back(&custom_input);
        return inputs;
    }

private:
    std::vector<TestInputRandom> random_inputs;
    std::vector<TestInputCustom> custom_inputs;
};

template<typename TestInput, typename TestInputRandom, typename TestInputCustom>
inline GeneratedTestInputs<TestInput, TestInputRandom, TestInputCustom>
    make_generated_test_inputs(std::vector<TestInputRandom>&& random_inputs,
                               std::vector<TestInputCustom>&& custom_inputs)
{
    return GeneratedTestInputs<TestInput, TestInputRandom, TestInputCustom>(
            std::move(random_inputs), std::move(custom_inputs));
}

}
