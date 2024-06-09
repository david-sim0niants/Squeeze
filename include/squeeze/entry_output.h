#pragma once

#include <variant>
#include <fstream>

#include "entry_header.h"

namespace squeeze {

class EntryOutput {
public:
    virtual Error<EntryOutput> init(const EntryHeader& entry_header, std::ostream *& stream) = 0;
    virtual Error<EntryOutput> init_symlink(
            const EntryHeader& entry_header, const std::string& target) = 0;
    virtual void deinit() noexcept = 0;

    virtual ~EntryOutput() = default;
};


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
