#pragma once

#include <variant>
#include <fstream>

#include "entry_header.h"

namespace squeeze {

/* EntryOutput is the interface responsible for obtaining an entry header from
 * the Reader and providing a target for the
 * extracted from the writer class. This is the abstract class of it.
 * Other derivations of it could store the obtained data in a file or a custom stream/string. */
class EntryOutput {
public:
    /* Initialize the entry output to represent an optional pointer to the stream content and
     * get references to the entry header and the optional stream pointer. */
    virtual Error<EntryOutput> init(const EntryHeader& entry_header, std::ostream *& stream) = 0;
    /* Initialize the entry output to represent a symlink target and
     * get references to the entry header and the optional stream pointer. */
    virtual Error<EntryOutput> init_symlink(
            const EntryHeader& entry_header, const std::string& target) = 0;
    virtual void deinit() noexcept = 0;

    virtual ~EntryOutput() = default;
};


/* FileEntryOutput creates a file for storing the extracted data.  */
class FileEntryOutput : public EntryOutput {
public:
    virtual Error<EntryOutput> init(const EntryHeader& entry_header, std::ostream *& stream) override;
    virtual Error<EntryOutput> init_symlink(
            const EntryHeader& entry_header, const std::string& target) override;
    virtual void deinit() noexcept override;

private:
    std::optional<std::ofstream> file;
};


class CustomStreamEntryOutput : public EntryOutput {
public:
    explicit CustomStreamEntryOutput(std::ostream& stream)
        : stream(stream)
    {}

    virtual Error<EntryOutput> init(const EntryHeader& entry_header, std::ostream *& stream) override;
    virtual Error<EntryOutput> init_symlink(
            const EntryHeader& entry_header, const std::string& target) override;
    virtual void deinit() noexcept override;

private:
    std::ostream& stream;
};

}
