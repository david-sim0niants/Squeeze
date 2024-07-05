#pragma once

#include <fstream>
#include <variant>

#include "entry_common.h"
#include "entry_header.h"
#include "error.h"
#include "compression/params.h"

namespace squeeze {

using compression::CompressionParams;

/* Interface responsible for providing the data (entry header and content) required
 * for the Writer interface to perform append operation. This is the abstract class of it.
 * Other derivations of it could provide the data from a file or in a custom way. */
class EntryInput {
public:
    /* The content type.
     * A monostate (no state) usually refers to a directory as directories don't have any content.
     * An input stream usually refers to a regular file contents.
     * A string usually refers to a symlink target. */
    using ContentType = std::variant<std::monostate, std::istream *, std::string>;

    virtual ~EntryInput() = default;

    /* Initialize the entry input and get references to the entry header and its content. */
    virtual Error<EntryInput> init(EntryHeader& entry_header, ContentType& content) = 0;
    virtual void deinit() noexcept = 0;

    inline const std::string& get_path() const
    {
        return path;
    }

protected:
    EntryInput(std::string&& path) : path(std::move(path))
    {}

    void init_entry_header(EntryHeader& entry_header);

    std::string path;
};


/* Entry input base class for all classes that store the path, compression_method and compression_level
 * before being initialized. */
class BasicEntryInput : public EntryInput {
protected:
    BasicEntryInput(std::string&& path, const CompressionParams& compression)
        :   EntryInput(std::move(path)), compression(compression)
    {}

    virtual ~BasicEntryInput() = default;

    void init_entry_header(EntryHeader& entry_header);

    CompressionParams compression;
};


/* Entry input interface that provides the entry header and content by opening
 * the file with the specified path. */
class FileEntryInput : public BasicEntryInput {
public:
    FileEntryInput(std::string&& path, const CompressionParams& compression)
        :   BasicEntryInput(std::move(path), compression)
    {}

    virtual Error<EntryInput> init(EntryHeader& entry_header, ContentType& content) override;
    virtual void deinit() noexcept override;

protected:
    Error<EntryInput> init_entry_header(EntryHeader& entry_header);

    std::optional<std::ifstream> file;
};


/* Entry input interface that contains the necessary entry header data and contents in advance. */
class CustomContentEntryInput : public BasicEntryInput {
public:
    CustomContentEntryInput(std::string&& path, const CompressionParams& compression,
            ContentType&& content, EntryAttributes entry_attributes = default_attributes)
        :
            BasicEntryInput(std::move(path), compression),
            content(std::move(content)), entry_attributes(entry_attributes)
    {}

    virtual Error<EntryInput> init(EntryHeader& entry_header, ContentType& content) override;
    virtual void deinit() noexcept override;

public:
    static constexpr EntryAttributes default_attributes = {
        EntryType::None,
        EntryPermissions::OwnerRead | EntryPermissions::OwnerWrite |
        EntryPermissions::GroupRead | EntryPermissions::GroupWrite |
        EntryPermissions::OthersRead | EntryPermissions::OthersWrite,
    };

protected:
    void init_entry_header(EntryHeader& entry_header);

    ContentType content;
    EntryAttributes entry_attributes;
};

}
