#include "squeeze/squeeze.h"
#include "utils/argparser.h"

#include <cstring>
#include <iostream>
#include <deque>
#include <filesystem>

static void usage();

static void process_sqz_instructions(squeeze::Squeeze& sqz, int argc, char *argv[]);

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage();
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];

    std::fstream sqz_file;
    std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out | std::ios_base::binary;
    if (!std::filesystem::exists(filename))
        mode |= std::ios_base::trunc;
    sqz_file.open(filename, mode);
    if (!sqz_file) {
        std::cerr << "Failed opening a file - " << filename << std::endl;
        return EXIT_FAILURE;
    }

    std::deque<squeeze::Error<squeeze::Writer>> write_errors;
    squeeze::Squeeze sqz(sqz_file);

    process_sqz_instructions(sqz, argc - 2, argv + 2);

    return EXIT_SUCCESS;
}

static void usage()
{
    std::cerr <<
R""""(Usage: sqz <sqz-file> <files...> [-options]
By default the append mode is enabled, so even without specifying -a or --append
at first, the files listed after the sqz file are assumed to be appended or updated.
Options:
    -a, --append       Append (or update) the following files to the sqz file
    -r, --remove       Remove the following files from the sqz file
    -x, --extract      Extract the following files from the sqz file
    -l, --list         List files in the sqz file
)"""";
}

enum class Option {
    None, Append, Remove, Extract, List
};

static Option parse_short_option(char o)
{
    switch (o) {
    case 'A':
        return Option::Append;
    case 'R':
        return Option::Remove;
    case 'X':
        return Option::Extract;
    case 'L':
        return Option::List;
    default:
        return Option::None;
    }
}

static Option parse_long_option(std::string_view option)
{
    if (option == "append")
        return Option::Append;
    if (option == "remove")
        return Option::Remove;
    if (option == "extract")
        return Option::Extract;
    if (option == "list")
        return Option::List;
    return Option::None;
}

static void process_sqz_instructions(squeeze::Squeeze& sqz, int argc, char *argv[])
{
    using namespace squeeze::tools::utils;
    ArgParser parser(argc, argv, "ARXL", { "append", "remove", "extract", "list" });
    while (auto maybe_arg = parser.next()) {
        auto& arg = *maybe_arg;
        switch (arg.type) {
        case ArgType::Positional:
            // TODO: process positional argument
            break;
        case ArgType::ShortOption:
            // TODO: parse_short_option(arg.value[0])
            break;
        case ArgType::LongOption:
            // TODO: parse_long_option(arg.value)
            break;
        case ArgType::UnknownShortOption:
            std::cerr << "unknown option: -" << arg.value << '\n';
            break;
        case ArgType::UnknownLongOption:
            std::cerr << "unknown option: --" << arg.value << '\n';
            break;
        default:
            std::cerr << "unexpected argument: " << arg.value << '\n';
        }
    }
}

