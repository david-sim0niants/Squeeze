#pragma once

#include <fstream>

#include "entry_common.h"
#include "entry_header.h"
#include "error.h"
#include "compression_method.h"

namespace squeeze {

class EntryInput {
public:
    virtual Error<EntryInput> init(std::istream *& stream, EntryHeader& entry_header) = 0;
    virtual void deinit() = 0;

    virtual ~EntryInput() = default;

    inline const std::string get_path() const
    {
        return path;
    }

protected:
    EntryInput(std::string&& path) : path(std::move(path))
    {}

    void init_entry_header(EntryHeader& entry_header);

    std::string path;
};


class BasicEntryInput : public EntryInput {
protected:
    BasicEntryInput(std::string&& path, CompressionMethod compression_method, int compression_level)
        :   EntryInput(std::move(path)),
            compression_method(compression_method),
            compression_level(compression_level)
    {}

    void init_entry_header(EntryHeader& entry_header);

    CompressionMethod compression_method;
    int compression_level;
};


class FileEntryInput : public BasicEntryInput {
public:
    FileEntryInput(std::string&& path, CompressionMethod compression_method, int compression_level)
        :   BasicEntryInput(std::move(path), compression_method, compression_level)
    {}

    Error<EntryInput> init(std::istream *& stream, EntryHeader& entry_header) override;
    void deinit() override;

protected:
    Error<EntryInput> init_entry_header(EntryHeader& entry_header);

    std::string path;
    CompressionMethod compression_method;
    int compression_level;
    std::optional<std::ifstream> file;
};


class CustomStreamEntryInput : public BasicEntryInput {
public:
    CustomStreamEntryInput(std::string&& path, CompressionMethod compression_method, int compression_level,
            std::istream& stream, EntryAttributes entry_attributes = default_attributes)
        :   BasicEntryInput(std::move(path), compression_method, compression_level),
            stream(stream), entry_attributes(entry_attributes)
    {}

    Error<EntryInput> init(std::istream *& stream, EntryHeader& entry_header) override;
    void deinit() override;

public:
    static constexpr EntryAttributes default_attributes = {
        EntryType::None,
        EntryPermissions::OwnerRead | EntryPermissions::OwnerWrite |
        EntryPermissions::GroupRead | EntryPermissions::GroupWrite |
        EntryPermissions::OthersRead | EntryPermissions::OthersWrite,
    };

protected:
    void init_entry_header(EntryHeader& entry_header);

    std::istream& stream;
    EntryAttributes entry_attributes;
};

}
