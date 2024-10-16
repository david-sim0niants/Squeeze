# Squeeze

A fast, multithreaded C++ compression API and CLI tool.

## Description

### Summary

This is a file compression library and utility, supporting multiple methods of compression: `None`, `Huffman`, and `Deflate`. Features file append/remove/extract operations, file entries listing, and more.
Contains a header-only compression algorithms API that includes __Huffman__, __LZ77__, and a combination of them, __Deflate__. The compression algorithms are implemented from scratch using C++20.

### File Format

The compression API defines a compact `sqz` file format. The format consists of compressed file entries. Each entry starts with a header that contains a path, attributes (permissions and type), compression method and level, content size, and major/minor version of Squeeze used for making the entry. The header is immediately followed by the compressed content, with an exact size as specified in the header. As the format is just a list of entries, it's even possible to concatenate two sqz files together, and the resulting file would also be a valid sqz file containing entries from both files.

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

Almost all options in the tool run in an interpreted manner, so that specifying `-A` means files listed after the option until another option like `-X` should be appended. It's possible to list mixed read/write options, and sqz will perform them sequentially. Options don't affect files listed before them; for example, `-r` sets recursive mode, which means directories listed afterwards will be appended, removed, or extracted recursively but doesn't affect files listed before.

### API

The main interface begins with the `Squeeze` class, which inherits from `Reader` and `Writer` classes.

`Reader` inherits from `Extracter` and `Lister`.
`Writer` inherits from `Appender` and `Remover`.

`Appender` is responsible for compressing and appending file entries to the squeeze file. Expects only an output stream to operate on.
`Remover` is responsible for removing entries from the squeeze file. Expects both an input and output stream.

`Extracter` is responsible for extracting entries from the squeeze file. Expects only an input stream.
`Lister` is responsible for supporting iterations over file entries, using the special `EntryIterator`. Expects only an input stream.

The Squeeze API comes with a header-only compression algorithms API that includes __Huffman__, __LZ77__, __Deflate__, compression configs per level for each method, and a general interface for compressing data with one of the following methods.
* `None`: stores data as is, without any compression. Supported levels: __0__
* `Huffman`: Huffman encoding/decoding with 15-bit limited code lengths. Supported levels: __[1-8]__
* `Deflate`: Deflate compression algorithm, implemented per the [DEFLATE specification](https://datatracker.ietf.org/doc/html/rfc1951). Supported levels: __[0-8]__

As always, smaller compression levels are faster but weaker at compression; larger levels are slower but stronger at compression.
