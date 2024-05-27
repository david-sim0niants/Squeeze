#include "squeeze/squeeze.h"

#include <iostream>
#include <deque>
#include <filesystem>

static void usage();

int main(int argc, char *argv[])
{
    if (argc < 3) {
        usage();
        return EXIT_FAILURE;
    }

    std::string cmd = argv[1];
    const char *filename = argv[2];

    std::fstream file;
    std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out | std::ios_base::binary;
    if (!std::filesystem::exists(filename))
        mode |= std::ios_base::trunc;
    file.open(filename, mode);
    if (!file) {
        std::cerr << "Failed opening a file - " << filename << std::endl;
        return EXIT_FAILURE;
    }

    std::deque<squeeze::Error<squeeze::Writer>> write_errors;
    squeeze::Squeeze sqz(file);

    if (cmd == "add") {
        for (int i = 3; i < argc; ++i) {
            if (!std::filesystem::exists(std::filesystem::path(argv[i]))) {
                std::cerr << "Ignoring non-existent file - " << argv[i] << '\n';
                continue;
            }

            write_errors.emplace_back();
            sqz.will_append<squeeze::FileEntryInput>(write_errors.back(), std::string(argv[i]), squeeze::CompressionMethod::None, 0);
            for (auto err : write_errors) {
                if (err)
                    std::cerr << err.report() << '\n';
            }
        }
    } else if (cmd == "rm") {
        for (int i = 3; i < argc; ++i) {
            auto it = sqz.find_path(argv[i]);
            if (it == sqz.end()) {
                std::cerr << "Ignoring non-existent entry - " << argv[i] << '\n';
                continue;
            }
            write_errors.emplace_back();
            sqz.will_remove(it, &write_errors.back());
        }
    } else if (cmd == "ls") {
        for (auto [pos, entry_header] : sqz) {
            std::cout << entry_header.path << ' ' << squeeze::utils::stringify(entry_header.attributes) << std::endl;
        }
    }

    if (cmd == "add" || cmd == "rm")
        sqz.write();

    file.close();

    return EXIT_SUCCESS;
}

static void usage()
{
    std::cerr <<
R""""(Usage: sqz <command> <file> [<args>]
Commands:
    add <file>  <files to add>
    rm  <file>  <files to remove>
    ext <file> [<files to extract>]
    ls  <file>
)"""";
}
