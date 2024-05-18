#include <iostream>
#include <filesystem>


int main(int argc, char *argv[])
{
    if (argc < 2)
        return EXIT_FAILURE;
    if (!std::filesystem::exists(argv[1]))
        return EXIT_FAILURE;
    for (auto& entry : std::filesystem::directory_iterator {argv[1]})
        std::cout << entry.path().filename().string() << ' ';
    return EXIT_SUCCESS;
}
