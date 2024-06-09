#include "squeeze/squeeze.h"
#include "squeeze/exception.h"
#include "utils/argparser.h"

#include <cstring>
#include <cassert>
#include <iostream>
#include <deque>
#include <filesystem>
#include <unordered_set>


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
    };

    enum class Option {
        Append, Remove, Extract, List, Help
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
        if (exit_code != EXIT_SUCCESS)
            return exit_code;

        return EXIT_SUCCESS;
    }

    static constexpr char short_options[] = "ARXLh";
    static constexpr std::string_view long_options[] = {"append", "remove", "extract", "list", "help"};

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
            auto path_opt = preprocess_filepath(arg_value);
            if (!path_opt.has_value())
                return EXIT_FAILURE;
            auto& path = *path_opt;
            if (appendee_path_set.contains(path))
                break;

            assert(sqz.has_value());
            write_errors.emplace_back();
            appendee_path_set.insert(path);
            sqz->will_append<FileEntryInput>(write_errors.back(), std::move(path),
                    CompressionMethod::None, 0);
            state |= Dirty;
            break;
        }
        case Remove:
        {
            assert(sqz.has_value());
            auto it = sqz->find_path(arg_value);
            if (it == sqz->end()) {
                std::cerr << "Error: non-existing path: " << arg_value << '\n';
                break;
            }
            write_errors.emplace_back();
            sqz->will_remove(it, &write_errors.back());
            state |= Dirty;
            break;
        }
        case Extract:
        {
            assert(sqz.has_value());
            Error<Reader> err = sqz->extract(arg_value);
            if (err)
                std::cerr << err.report() << '\n';
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

        return EXIT_SUCCESS;
    }

    int deinit_sqz()
    {
        if (!sqz)
            return EXIT_SUCCESS;
        int exit_code = run_update();
        sqz.reset();
        sqz_file.close();
        sqz_fn.clear();
        return exit_code;
    }

    int run_update()
    {
        if (!(state & Dirty))
            return EXIT_SUCCESS;

        sqz->update();
        int exit_code = EXIT_SUCCESS;
        for (auto err : write_errors) {
            if (err) {
                exit_code = EXIT_FAILURE;
                std::cerr << err.report() << '\n';
            }
        }
        write_errors.clear();
        appendee_path_set.clear();
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
        if (option == "help")
            return Option::Help;
        throw BaseException("unexpected long option");
    }

    static std::optional<std::string> preprocess_filepath(const std::string_view path_str)
    {
        std::filesystem::path path = std::filesystem::path(path_str).lexically_normal();
        std::error_code ec;
        auto status = std::filesystem::symlink_status(path, ec);
        switch (status.type()) {
            using enum std::filesystem::file_type;
        case directory:
            path = path / "";
            break;
        case not_found:
            std::cerr << "Error: no such file or directory - " << path_str << std::endl;
            return std::nullopt;
        case unknown:
            std::cerr << "Error: unknown file type - " << path_str << std::endl;
            return std::nullopt;
        default:
            break;
        }
        return path;
    }

    static void usage()
    {
        std::cerr <<
R""""(Usage: sqz <sqz-file> <files...> [-options]
By default the append mode is enabled, so even without specifying -A or --append
at first, the files listed after the sqz file are assumed to be appended or updated.
Options:
    -A, --append       Append (or update) the following files to the sqz file
    -R, --remove       Remove the following files from the sqz file
    -X, --extract      Extract the following files from the sqz file
    -L, --list         List files in the sqz file
)"""";
    }

private:
    std::filesystem::path sqz_fn;
    std::fstream sqz_file;
    std::optional<Squeeze> sqz;
    std::optional<ArgParser> arg_parser;
    ModeFlags mode = Append;
    int state = 0;
    std::unordered_set<std::string> appendee_path_set;
    std::deque<Error<Writer>> write_errors;
};

}


int main(int argc, char *argv[])
{
    return CLI().run(argc, argv);
}
