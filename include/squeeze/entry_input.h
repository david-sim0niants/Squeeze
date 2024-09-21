#pragma once

#include <fstream>
#include <variant>

#include "entry_common.h"
#include "entry_header.h"
#include "status.h"
#include "compression/params.h"

namespace squeeze {

using compression::CompressionParams;

/* Abstract class used by append operations as a source to read from for appending contents.
 * Could as well be called EntrySource or something. */
class EntryInput {
public:
    /* Status return type of this class methods. */
    using Stat = StatStr;

    /* The content type.
     * A monostate (no state) mainly refers to a directory as directories don't have any content.
     * An input stream mainly refers to a regular file contents.
     * A string mainly refers to a symlink target. */
    using ContentType = std::variant<std::monostate, std::istream *, std::string>;

    virtual ~EntryInput() = default;

    /* Initialize the entry input and get references to the entry header and its content. */
    virtual Stat init(EntryHeader& entry_header, ContentType& content) = 0;
    /* De-initialize the entry output. This always needs to be called if an init method
     * has been called, including exception handling cases. */
    virtual void deinit() noexcept = 0;

    inline const std::string& get_path() const
    {
        return path;
    }

protected:
    EntryInput(std::string&& path) : path(std::move(path))
    {
    }

    void init_entry_header(EntryHeader& entry_header);

    std::string path;
};

/* Entry input base class for all classes that store the path and compression params
 * before being initialized. */
class BasicEntryInput : public EntryInput {
protected:
    BasicEntryInput(std::string&& path, const CompressionParams& compression)
        :   EntryInput(std::move(path)), compression(compression)
    {
    }

    virtual ~BasicEntryInput() = default;

    void init_entry_header(EntryHeader& entry_header);

    CompressionParams compression;
};

/* Derived class of BasicEntryInput that opens a file for reading contents from. */
class FileEntryInput : public BasicEntryInput {
public:
    FileEntryInput(std::string&& path, const CompressionParams& compression)
        :   BasicEntryInput(std::move(path), compression)
    {
    }

    virtual Stat init(EntryHeader& entry_header, ContentType& content) override;
    virtual void deinit() noexcept override;

protected:
    Stat init_entry_header(EntryHeader& entry_header);

    std::optional<std::ifstream> file;
};

/* Derived class of BasicEntryInput that relies on a pre-existing content and entry header to read from. */
class CustomContentEntryInput : public BasicEntryInput {
public:
    CustomContentEntryInput(std::string&& path, const CompressionParams& compression,
            ContentType&& content, EntryAttributes entry_attributes = default_attributes)
        :
            BasicEntryInput(std::move(path), compression),
            content(std::move(content)), entry_attributes(entry_attributes)
    {
    }

    virtual Stat init(EntryHeader& entry_header, ContentType& content) override;
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

protected:
    ContentType content;
    EntryAttributes entry_attributes;
};

}
