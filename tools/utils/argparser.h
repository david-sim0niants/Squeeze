#pragma once

#include <vector>
#include <string>
#include <optional>
#include <algorithm>

namespace squeeze::tools::utils {

enum class ArgType {
    None = 0, Positional, ShortOption, LongOption, UnknownShortOption, UnknownLongOption
};

struct Arg {
    ArgType type;
    std::string_view value;
};

class ArgParser {
public:
    ArgParser(int argc, const char * const argv[],
            std::string&& short_options,
            std::vector<std::string>&& long_options)
        : argc(argc), argv(argv), short_options(std::move(short_options)), long_options(std::move(long_options))
    {}

    void reset()
    {
        curr_arg = nullptr;
        curr_arg_index = 0;
        curr_arg_type = ArgType::None;
    }

    std::optional<Arg> next()
    {
        if (curr_arg_type == ArgType::ShortOption && *++curr_arg) {
            const bool valid_option = short_options.find(*curr_arg) != std::string::npos;
            return Arg {valid_option ? ArgType::ShortOption : ArgType::UnknownShortOption,
                        std::string_view(curr_arg, 1)};
        }

        if (++curr_arg_index >= argc)
            return std::nullopt;
        curr_arg = argv[curr_arg_index];

        if (!curr_arg && !*curr_arg)
            return next();

        if (*curr_arg != '-') {
            curr_arg_type = ArgType::Positional;
            return Arg{ ArgType::Positional, curr_arg };
        }

        if (curr_arg[1] != '-') {
            if (!curr_arg[1]) {
                const bool valid_option = short_options.find('\0') != std::string::npos;
                return Arg {valid_option ? ArgType::ShortOption : ArgType::UnknownShortOption,
                            std::string_view(curr_arg + 1, 1)};
            }

            curr_arg_type = ArgType::ShortOption;
            return next();
        }

        curr_arg_type = ArgType::LongOption;
        std::string_view current_arg_val = curr_arg + 2;

        const bool valid_option = std::find_if(long_options.begin(), long_options.end(),
                [&current_arg_val](const auto& option)
                {
                    return current_arg_val == option;
                }
            ) != long_options.end();
        return Arg {valid_option ? ArgType::LongOption : ArgType::UnknownLongOption, current_arg_val};
    }

private:
    int argc;
    const char *const *argv;
    std::string short_options;
    std::vector<std::string> long_options;

    const char *curr_arg;
    int curr_arg_index = -1;
    ArgType curr_arg_type = ArgType::None;
};

}
