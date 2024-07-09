#pragma once

#include <variant>
#include <fstream>

#include "entry_header.h"

namespace squeeze {

/* Abstract class used by extract operations as a sink to write the extracted entry contents to.
 * Could as well be called EntrySink or something. */
class EntryOutput {
public:
    /* Initialize the entry output by providing an entry header. Should either return a pointer
     * to an output stream if it exists, NULL if it doesn't (and not supposed to) or an error. */
    virtual Error<EntryOutput> init(const EntryHeader& entry_header, std::ostream *& stream) = 0;
    /* Initialize the entry output as a symlink by passing entry header and a symlink target. */
    virtual Error<EntryOutput> init_symlink(
            const EntryHeader& entry_header, const std::string& target) = 0;
    /* Finalize the entry output. This can be used to do some post-processing tasks. */
    virtual Error<EntryOutput> finalize() = 0;
    /* De-initialize the entry output. This always needs to be called if an init(_symlink) method
     * has been called, including exception handling cases. */
    virtual void deinit() noexcept = 0;

    virtual ~EntryOutput() = default;
};

/* Derived class of EntryOutput that creates a file for storing extracted data. */
class FileEntryOutput : public EntryOutput {
public:
    virtual Error<EntryOutput> init(const EntryHeader& entry_header, std::ostream *& stream) override;
    virtual Error<EntryOutput> init_symlink(
            const EntryHeader& entry_header, const std::string& target) override;
    virtual Error<EntryOutput> finalize() override;
    virtual void deinit() noexcept override;

private:
    std::optional<std::ofstream> file;
    std::optional<EntryHeader> final_entry_header;
};

/* Derived class of EntryOutput that relies on a pre-existing stream for storing extracted data.*/
class CustomStreamEntryOutput : public EntryOutput {
public:
    explicit CustomStreamEntryOutput(std::ostream& stream)
        : stream(stream)
    {}

    virtual Error<EntryOutput> init(const EntryHeader& entry_header, std::ostream *& stream) override;
    virtual Error<EntryOutput> init_symlink(
            const EntryHeader& entry_header, const std::string& target) override;
    virtual Error<EntryOutput> finalize() override
    {
        return success;
    }
    virtual void deinit() noexcept override;

private:
    std::ostream& stream;
};

}
