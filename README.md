# Squeeze

A fast, multithreaded C++ compression library and utility. Go to [description](#description) for more info.

## Build and Test

CMake is required to be installed.

The only dependency Squeeze may have is [spdlog](https://github.com/gabime/spdlog). If logging is not needed, pass `-DUSE_LOGGING=OFF` to CMake. Otherwise, spdlog must be installed and findable by `find_package()`.

To enable tests, pass `-DBUILD_TESTS=ON` to CMake.

Clone the repo and, depending on the system you're running, choose the appropriate build procedure.
### Linux or Unix-like
`make` is required to be installed.
```sh
mkdir build
cd build
cmake ../ -DCMAKE_BUILD_TYPE=Release # or any other build type
make -j$(nproc)
```
#### Run the CLI tool
```sh
./tools/sqz
```
#### Test
Assuming testing was enabled:
```
./test/test_all
```

### Windows
Visual Studio is required to be installed.
```sh
mkdir build
cd build
cmake ../
cmake --build . --config Release # or any other build type
```
#### Run the CLI tool
```sh
./bin/Release/sqz
```
#### Test
Assuming testing was enabled:
```
./bin/Release/test_all
```

## Description

### Summary

This is a file compression library and utility, supporting multiple compression methods: `None`, `Huffman`, and `Deflate`. It features operations for appending, removing, and extracting files, as well as listing file entries and more. The library comes with a header-only collection of compression algorithms that includes __Huffman__, __LZ77__, and __Deflate__. The compression algorithms are implemented from scratch using C++20 features.

### File Format

The library defines a compact `*.sqz` file format. The format consists of compressed file entries. Each entry begins with a header that includes the path, attributes (permissions and type), compression method and level, content size, and version of Squeeze used to create the entry. The header is followed by the entry content, which has the size specified in the header. Since the format is just a list of entries, it is also possible to concatenate two sqz files, resulting in a valid sqz file that contains entries from both files.

### CLI Tool

Below is the general usage of the tool, shown when running the `sqz` command without arguments or with the `-h, --help` option:
```
Usage: sqz <sqz-file> <files...> [-options]
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
```

Most options in the tool operate in an interpreted manner, so that specifying `-A` means files listed after the option until another option like `-X` should be appended. It's possible to list mixed read/write options, and sqz will perform them sequentially. Options don't affect files listed before them; for example, `-r` sets recursive mode, which means directories listed afterwards will be appended, removed, or extracted recursively but doesn't affect files listed before.

### Library

The compression library is centered around the `Squeeze` class, which combines the functionality of both reading and writing operations. `Squeeze` inherits from two key components: the `Reader` and `Writer` classes.

* `Reader` inherits from `Extracter` and `Lister`.
    * `Extracter` handles extracting entries,
    * `Lister` supports listing entries and includes `begin()` and `end()` methods, allowing iteration over the compressed data using an `EntryIterator`. This makes the compressed data format behave like an STL container, enabling users to traverse entries as if they were interacting with a standard container.
* Writer inherits from `Appender` and `Remover`.
    * `Appender` handles adding new entries,
    * `Remover` manages the removal of entries.

`Reader` performs all reading-related operations, while `Writer` manages writing-related tasks, and `Squeeze` can handle both. `Reader` and its base classes expect an input stream for reading. `Writer` and `Remover` expect an I/O stream to perform both reading and writing. `Appender` only requires an output stream.

Additionally, there are specialized wrapper classes (`FileSqueeze`, `FileAppender`, `FileRemover`, `FileExtracter`) designed for file-specific operations. These wrappers extend the functionality of `Squeeze` to interact directly with the filesystem, enabling features like recursive file extraction and appending from directories. While the core `Squeeze` class and its base classes work with abstract streams using `EntryInput` and `EntryOutput` for appending or extracting data, the wrapper classes handle actual file I/O.

The Squeeze API includes a header-only compression library featuring algorithms such as __Huffman__, __LZ77__, and __Deflate__. It also provides configurable compression levels for each method and a unified interface for compressing data using one of the following methods:

* `None`: Stores data without any compression. Supported level: 0
* `Huffman`: Implements Huffman encoding/decoding with 15-bit limited code lengths. Supported levels: [1-8]
* `Deflate`: Implements the Deflate compression algorithm according to the [DEFLATE](https://datatracker.ietf.org/doc/html/rfc1951) specification. Supported levels: [0-8]

As usual, lower compression levels offer faster performance but weaker compression, while higher levels provide stronger compression at the cost of speed.

