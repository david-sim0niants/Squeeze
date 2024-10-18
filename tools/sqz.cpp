// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include <cstring>
#include <cassert>
#include <iostream>
#include <deque>
#include <filesystem>
#include <charconv>

#include "squeeze/squeeze.h"
#include "squeeze/logging.h"
#include "squeeze/wrap/file_squeeze.h"
#include "squeeze/exception.h"
#include "squeeze/compression/config.h"
#include "squeeze/utils/defer.h"
#include "squeeze/utils/defer_macros.h"

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
        FileCreated = 1,
        Processing = 2,
        Dirty = 4,
        RecurseFlag = 8,
    };

    enum class Option {
        Append, Remove, Extract, List, Recurse, NoRecurse, Compression, LogLevel, Help
    };

public:
    int run(int argc, char *argv[])
    {
        if (argc < 2) {
            usage();
            return EXIT_FAILURE;
        }

        init_logging();

        arg_parser.emplace(argc - 1, argv + 1,
                short_options, long_options, long_options + std::size(long_options));
        int exit_code = handle_arguments();
        arg_parser.reset();

        state.flags = 0;
        state.compression = default_compression;
        mode = Append;

        return exit_code;
    }

    static constexpr compression::CompressionParams default_compression = {
        .method = compression::CompressionMethod::Deflate,
        .level = 8,
    };

    static constexpr char short_options[] = "ARXLhrCl";
    static constexpr std::string_view long_options[] = {"append", "remove", "extract", "list", "help", "recurse", "no-recurse", "compression", "log-level"};

private:
    int handle_arguments()
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

        if (!(state.flags & Processing)) {
            exit_code = init_sqz(arg_value);
            if (exit_code != EXIT_SUCCESS)
                return exit_code;
            state.flags |= Processing;
            return EXIT_SUCCESS;
        }

        if (Read & mode) {
            if ((exit_code = run_update()))
                return exit_code;
        }

        switch (mode) {
        case Append:
            exit_code = handle_append(arg_value);
            break;
        case Remove:
            exit_code = handle_remove(arg_value);
            break;
        case Extract:
            exit_code = handle_extract(arg_value);
            break;
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
            if (!(state.flags & Processing)) {
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
            state.flags |= RecurseFlag;
            break;
        case Option::NoRecurse:
            state.flags &= ~RecurseFlag;
            break;
        case Option::Compression:
        {
            auto arg = arg_parser->raw_next();
            if (!arg) {
                std::cerr << "Error: no compression info specified.\n";
                return EXIT_FAILURE;
            }
            if (not parse_compression_params(*arg, state.compression))
                return EXIT_FAILURE;
            break;
        }
        case Option::LogLevel:
        {
            auto arg = arg_parser->raw_next();
            if (!arg) {
                std::cerr << "Error: no log level specified.\n";
                return EXIT_FAILURE;
            }
            LogLevel log_level;
            if (not parse_log_level(*arg, log_level))
                return EXIT_FAILURE;

            set_log_level(log_level);

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

    int handle_append(const std::string_view path)
    {
        assert(sqz.has_value());
        assert(fsqz.has_value());
        state.flags |= Dirty;

        if (state.flags & RecurseFlag) {
            fsqz->will_append_recursively(path, state.compression, make_back_inserter_lambda(write_stats));
        } else {
            write_stats.emplace_back();
            fsqz->will_append(path, state.compression, &write_stats.back());
        }

        return EXIT_SUCCESS;
    }

    int handle_remove(const std::string_view path)
    {
        assert(sqz.has_value());
        assert(fsqz.has_value());
        state.flags |= Dirty;

        if (path == "*") {
            fsqz->will_remove_all(make_back_inserter_lambda(write_stats));
            return EXIT_SUCCESS;
        }

        if (state.flags & RecurseFlag) {
            fsqz->will_remove_recursively(path, make_back_inserter_lambda(write_stats));
        } else {
            write_stats.emplace_back();
            fsqz->will_remove(path, &write_stats.back());
        }

        return EXIT_SUCCESS;
    }

    int handle_extract(const std::string_view path)
    {
        assert(sqz.has_value());
        assert(fsqz.has_value());

        if (path == "*") {
            std::deque<Reader::Stat> read_stats;
            fsqz->extract_all(make_back_inserter_lambda(read_stats));
            for (auto& stat : read_stats)
                if (stat.failed())
                    print_to(std::cerr, stat, '\n');
            return EXIT_SUCCESS;
        }

        if (state.flags & RecurseFlag) {
            std::deque<Reader::Stat> read_stats;
            fsqz->extract_recursively(path, make_back_inserter_lambda(read_stats));
            for (auto& stat : read_stats)
                if (stat.failed())
                    print_to(std::cerr, stat, '\n');
        } else {
            Reader::Stat stat = fsqz->extract(path);
            if (stat.failed())
                print_to(std::cerr, stat, '\n');
        }

        return EXIT_SUCCESS;
    }

    int init_sqz(std::string_view filename)
    {
        deinit_sqz();
        sqz_fn = std::filesystem::path(filename);

        std::ios_base::openmode file_mode = std::ios_base::in | std::ios_base::out | std::ios_base::binary;
        if (!std::filesystem::exists(sqz_fn)) {
            file_mode |= std::ios_base::trunc;
            state.flags |= FileCreated;
        }

        sqz_file.open(sqz_fn, file_mode);
        if (!sqz_file) {
            std::cerr << "Error: failed opening a file - " << sqz_fn << '\n';
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

        bool delete_file = false;
        if (state.flags & FileCreated) {
            sqz_file.seekp(0, std::ios_base::end);
            if (sqz_file.tellp() == 0)
                delete_file = true;
            state.flags &= ~FileCreated;
        }

        sqz_file.close();

        if (delete_file)
            std::filesystem::remove(sqz_fn);

        sqz_fn.clear();
        return exit_code;
    }

    int run_update()
    {
        if (!(state.flags & Dirty))
            return EXIT_SUCCESS;

        int exit_code = EXIT_SUCCESS;
        fsqz->update();

        for (auto& stat : write_stats) {
            if (stat.failed()) {
                exit_code = EXIT_FAILURE;
                print_to(std::cerr, stat, '\n');
            }
        }
        write_stats.clear();

        std::filesystem::resize_file(sqz_fn, sqz_file.tellp());

        state.flags &= ~Dirty;
        return exit_code;
    }

    void run_list()
    {
        assert(sqz.has_value());

        LogLevel log_level = get_log_level();
        set_log_level(LogLevel::Off);
        DEFER( set_log_level(log_level) );

        for (auto it = sqz->begin(); it != sqz->end(); ++it) {
            const auto& entry_header = it->second;
            std::stringstream extra;
            if (entry_header.attributes.get_type() == EntryType::Symlink) {
                extra << " -> ";
                sqz->extract(it, extra);
            }
            print_to(std::cout, entry_header.attributes, "    ", entry_header.path, extra.view(), '\n');
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
        case 'C':
            return Option::Compression;
        case 'l':
            return Option::LogLevel;
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
        if (option == "compression")
            return Option::Compression;
        if (option == "log-level")
            return Option::LogLevel;
        if (option == "help")
            return Option::Help;
        throw BaseException("unexpected long option");
    }

    static bool parse_compression_params(std::string_view str, compression::CompressionParams& params)
    {
        if (str.empty())
            return false;

        using namespace std::string_view_literals;
        using enum compression::CompressionMethod;

        constexpr std::array str_methods = {"none"sv, "huffman"sv, "deflate"sv};
        constexpr std::array methods = {None, Huffman, Deflate};

        auto report_invalid_info = [str]()
        {
            std::cerr << "Error: invalid compression info specified - " << str << '\n';
        };

        compression::CompressionParams new_params = params;

        bool compr_method_set = false;
        for (std::size_t i = 0; i < str_methods.size(); ++i) {
            if (str.starts_with(str_methods[i])) {
                new_params.method = methods[i];
                new_params.level = compression::get_min_max_levels(new_params.method).first;
                str = str.substr(str_methods[i].size());
                compr_method_set = true;
                break;
            }
        }

        if (str.empty()) {
            params = new_params;
            return true;
        }

        if (compr_method_set) {
            if (str.front() != '/') {
                report_invalid_info();
                return false;
            }
            str = str.substr(1);
        }

        if (str.empty()) {
            report_invalid_info();
            return false;
        }

        if (std::isdigit(str.front())) {
            uint8_t level = 0;
            std::from_chars_result result = std::from_chars(str.data(), str.data() + str.size(), level);
            switch (result.ec) {
            case std::errc::invalid_argument:
            case std::errc::result_out_of_range:
                std::cerr << "Error: out of range compression level value.\n";
                return false;
            default:
                break;
            }

            auto [min_level, max_level] = compression::get_min_max_levels(new_params.method);
            if (level < min_level or level > max_level) {
                std::cerr << "Error: compression level is out of range - " << (int)level
                          << ". Min: " << (int)min_level << " Max: " << (int)max_level << '\n';
                return false;
            }

            new_params.level = level;
        } else if (not compr_method_set) {
            report_invalid_info();
            return false;
        }

        params = new_params;
        return true;
    }

    static bool parse_log_level(std::string_view str, LogLevel& log_level)
    {
        std::array<char, 9> upper_str {};
        for (int i = 0; i < std::min(9, (int)str.size()); ++i)
            upper_str[i] = toupper(str[i]);

        using enum squeeze::LogLevel;
        static const std::unordered_map<std::string_view, squeeze::LogLevel> log_level_map =
        {
            { "TRACE",    Trace    }, { "T", Trace    }, { "0", Trace    },
            { "DEBUG",    Debug    }, { "D", Debug    }, { "1", Debug    },
            { "INFO",     Info     }, { "I", Info     }, { "2", Info     },
            { "WARN",     Warn     }, { "W", Warn     }, { "3", Warn     },
            { "ERROR",    Error    }, { "E", Error    }, { "4", Error    },
            { "CRITICAL", Critical }, { "C", Critical }, { "5", Critical },
            { "OFF",      Off      }, { "O", Off      }, { "6", Off      },
        };

        auto it = log_level_map.find(upper_str.data());
        if (it == log_level_map.end()) {
            std::cerr << "Error: invalid log level - " << str << std::endl;
            return false;
        }
        log_level = it->second;
        return true;
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
    -L, --list          List all entries in the sqz file
    -r, --recurse       Enable recursive mode: directories will be processed recursively
        --no-recurse    Disable non-recursive mode: directories won't be processed recursively
    -C, --compression   Specify compression info in the form of 'method' or 'level' or 'method/level',
                        where method is one of the following: {none, huffman, deflate}; and level is an integer with
                        the following bounds for each method: {[0-0], [1-8], [0-8]}
    -l, --log-level     Set the log level which can be one of the following:
                        [trace, debug, info, warn, error, critical, off] or:
                        [0,     1,     2,    3,    4,     5,        6  ] or:
                        [t,     d,     i,    w,    e,     c,        o  ]
                        Log levels are case-insensitive
    -h, --help          Display usage information
)"""";
    }

private:
    ModeFlags mode = Append;
    struct State {
        int flags = 0;
        compression::CompressionParams compression = default_compression;
    } state;

    std::optional<ArgParser> arg_parser;
    std::filesystem::path sqz_fn;
    std::fstream sqz_file;
    std::optional<Squeeze> sqz;
    std::optional<wrap::FileSqueeze> fsqz;
    std::deque<Writer::Stat> write_stats;
};

}

int main(int argc, char *argv[])
{
    return CLI().run(argc, argv);
}
