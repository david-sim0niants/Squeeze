#include <cstring>
#include <cassert>
#include <iostream>
#include <deque>
#include <filesystem>

#include "squeeze/squeeze.h"
#include "squeeze/wrap/file_squeeze.h"
#include "squeeze/exception.h"

#include "utils/argparser.h"

namespace {

using namespace squeeze;
using namespace squeeze::tools::utils;

class CLI {
public:
    enum ModeFlags {
        Append = 1,
        Remove = 2,
        Extract = 4,
        Read = Extract,
        Write = Append | Remove,
    };

    enum StateFlags {
        Processing = 1,
        Dirty = 2,
        RecurseFlag = 4,
    };

    enum class Option {
        Append, Remove, Extract, List, Recurse, NoRecurse, Help
    };

public:
    int run(int argc, char *argv[])
    {
        if (argc < 2) {
            usage();
            return EXIT_FAILURE;
        }

        arg_parser.emplace(argc - 1, argv + 1,
                short_options, long_options, long_options + std::size(long_options));
        int exit_code = run_instructions();
        arg_parser.reset();
        return exit_code;
    }

    static constexpr char short_options[] = "ARXLhr";
    static constexpr std::string_view long_options[] = {"append", "remove", "extract", "list", "help", "recurse", "no-recurse"};

private:
    int run_instructions()
    {
        while (auto maybe_arg = arg_parser->next()) {
            auto& arg = *maybe_arg;
            int exit_code = EXIT_SUCCESS;
            switch (arg.type) {
            case ArgType::Positional:
                exit_code = handle_positional_argument(arg.value);
                break;
            case ArgType::ShortOption:
                exit_code = handle_option(parse_short_option(arg.value.front()));
                break;
            case ArgType::LongOption:
                exit_code = handle_option(parse_long_option(arg.value));
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

            if (exit_code != EXIT_SUCCESS)
                return exit_code;
        }
        return deinit_sqz();
    }

    static auto make_back_inserter_lambda(auto& container)
    {
        return [&container]{ container.emplace_back(); return &container.back(); };
    }

    int handle_positional_argument(std::string_view arg_value)
    {
        int exit_code = EXIT_SUCCESS;

        if (!(state & Processing)) {
            exit_code = init_sqz(arg_value);
            if (exit_code != EXIT_SUCCESS)
                return exit_code;
            state |= Processing;
            return EXIT_SUCCESS;
        }

        if (Read & mode) {
            if ((exit_code = run_update()))
                return exit_code;
        }

        switch (mode) {
        case Append:
        {
            assert(sqz.has_value());
            if (state & RecurseFlag) {
                fsqz->will_append_recursively(
                        arg_value, CompressionMethod::None, 0,
                        make_back_inserter_lambda(write_errors));
            } else {
                write_errors.emplace_back();
                fsqz->will_append(arg_value, CompressionMethod::None, 0, &write_errors.back());
            }
            state |= Dirty;
            break;
        }
        case Remove:
        {
            assert(sqz.has_value());
            if (state & RecurseFlag) {
                fsqz->will_remove_recursively(arg_value, make_back_inserter_lambda(write_errors));
            } else {
                write_errors.emplace_back();
                fsqz->will_remove(arg_value, &write_errors.back());
            }
            state |= Dirty;
            break;
        }
        case Extract:
        {
            assert(sqz.has_value());
            if (state & RecurseFlag) {
                std::deque<Error<Reader>> read_errors;
                fsqz->extract_recursively(arg_value, make_back_inserter_lambda(read_errors));
                for (auto& err : read_errors)
                    if (err)
                        std::cerr << err.report() << '\n';
            } else {
                Error<Reader> err = sqz->extract(arg_value);
                if (err)
                    std::cerr << err.report() << '\n';
            }
            break;
        }
        default:
            throw BaseException("unexpected mode");
            break;
        }

        return exit_code;
    }

    int handle_option(Option option)
    {
        switch (option) {
        case Option::Append:
            mode = Append;
            break;
        case Option::Remove:
            mode = Remove;
            break;
        case Option::Extract:
            mode = Extract;
            break;
        case Option::List:
        {
            if (!(state & Processing)) {
                std::cerr << "Error: no file specified.\n";
                return EXIT_FAILURE;
            }
            int exit_code = run_update();
            if (exit_code != EXIT_SUCCESS)
                return exit_code;
            run_list();
            break;
        }
        case Option::Recurse:
            state |= RecurseFlag;
            break;
        case Option::NoRecurse:
            state &= ~RecurseFlag;
            break;
        case Option::Help:
            usage();
            break;
        default:
            throw BaseException("unexpected option");
        }

        return EXIT_SUCCESS;
    }

    int init_sqz(std::string_view filename)
    {
        deinit_sqz();
        sqz_fn = std::filesystem::path(filename);

        std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out | std::ios_base::binary;
        if (!std::filesystem::exists(sqz_fn))
            mode |= std::ios_base::trunc;

        sqz_file.open(sqz_fn, mode);
        if (!sqz_file) {
            std::cerr << "Error: failed opening a file - " << sqz_fn.c_str() << '\n';
            return EXIT_FAILURE;
        }

        sqz.emplace(sqz_file);

        if (sqz->is_corrupted())
            std::cerr << "WARNING: corrupted sqz file - " << sqz_fn << std::endl;

        fsqz.emplace(*sqz);

        return EXIT_SUCCESS;
    }

    int deinit_sqz()
    {
        if (!sqz)
            return EXIT_SUCCESS;
        int exit_code = run_update();
        fsqz.reset();
        sqz.reset();
        sqz_file.close();
        sqz_fn.clear();
        return exit_code;
    }

    int run_update()
    {
        if (!(state & Dirty))
            return EXIT_SUCCESS;

        fsqz->update();
        int exit_code = EXIT_SUCCESS;
        for (auto err : write_errors) {
            if (err) {
                exit_code = EXIT_FAILURE;
                std::cerr << err.report() << '\n';
            }
        }
        write_errors.clear();
        std::filesystem::resize_file(sqz_fn, sqz_file.tellp());

        state &= ~Dirty;
        return exit_code;
    }

    void run_list()
    {
        assert(sqz.has_value());
        for (auto it = sqz->begin(); it != sqz->end(); ++it) {
            const auto& entry_header = it->second;
            std::cout << squeeze::utils::stringify(entry_header.attributes)
                      << "    " << entry_header.path;
            if (entry_header.attributes.type == EntryType::Symlink) {
                std::cout << " -> ";
                sqz->extract(it, std::cout);
            }
            std::cout << std::endl;
        }
    }

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
        case 'r':
            return Option::Recurse;
        case 'h':
            return Option::Help;
        default:
            throw BaseException("unexpected short option");
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
        if (option == "recurse")
            return Option::Recurse;
        if (option == "no-recurse")
            return Option::NoRecurse;
        if (option == "help")
            return Option::Help;
        throw BaseException("unexpected long option");
    }

    static void usage()
    {
        std::cerr <<
R""""(Usage: sqz <sqz-file> <files...> [-options]
By default the append mode is enabled, so even without specifying -A or --append
at first, the files listed after the sqz file are assumed to be appended or updated.
Options:
    -A, --append        Append (or update) the following files to the sqz file
    -R, --remove        Remove the following files from the sqz file
    -X, --extract       Extract the following files from the sqz file
    -L, --list          List files in the sqz file
    -r, --recurse       Enable recursive mode: directories will be processed recursively
        --no-recurse    Disable non-recursive mode: directories won't be processed recursively
    -h, --help          Display usage information
)"""";
    }

private:
    std::optional<ArgParser> arg_parser;
    std::filesystem::path sqz_fn;
    std::fstream sqz_file;
    std::optional<Squeeze> sqz;
    std::optional<wrap::FileSqueeze> fsqz;
    ModeFlags mode = Append;
    int state = 0;
    std::deque<Error<Writer>> write_errors;
};

}


int main(int argc, char *argv[])
{
    return CLI().run(argc, argv);
}
