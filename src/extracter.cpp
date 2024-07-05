#include "squeeze/extracter.h"

#include "squeeze/logging.h"
#include "squeeze/exception.h"
#include "squeeze/utils/io.h"
#include "squeeze/decoder.h"

namespace squeeze {

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::Extracter::"

Error<Extracter> Extracter::extract(const EntryIterator& it)
{
    FileEntryOutput entry_output;
    return extract(it, entry_output);
}

Error<Extracter> Extracter::extract(const EntryIterator& it, std::ostream& output)
{
    CustomStreamEntryOutput entry_output(output);
    return extract(it, entry_output);
}

Error<Extracter> Extracter::extract(const EntryIterator& it, EntryOutput& entry_output)
{
    SQUEEZE_TRACE("Extracting {}", it->second.path);

    auto& [pos, entry_header] = *it;
    source.seekg(pos + entry_header.get_encoded_header_size());

    SQUEEZE_DEBUG("Entry header: {}", utils::stringify(entry_header));

    switch (entry_header.attributes.type) {
        using enum EntryType;
    case None:
    case RegularFile:
    case Directory:
    {
        std::ostream *output;
        auto e = entry_output.init(entry_header, output);
        if (e) {
            SQUEEZE_ERROR("Failed initializing entry output");
            return {"failed initializing entry output", e.report()};
        }
        if (output)
            return extract_stream(entry_header, *output);
        break;
    }
    case Symlink:
    {
        std::string target;
        auto e = extract_symlink(entry_header, target);
        if (e) {
            SQUEEZE_ERROR("Failed extracting symlink");
            return {"failed extracting symlink", e.report()};
        }

        auto ee = entry_output.init_symlink(entry_header, target);
        if (ee) {
            SQUEEZE_ERROR("Failed extracting symlink");
            return {"failed extracting symlink", ee.report()};
        }

        break;
    }
    default:
        return Error<Extracter>("invalid entry type");
    }
    return success;
}

Error<Extracter> Extracter::extract_stream(const EntryHeader& entry_header, std::ostream& output)
{
    return decode(source, entry_header.content_size, output, entry_header.compression);
}

Error<Extracter> Extracter::extract_symlink(const EntryHeader& entry_header, std::string& target)
{
    SQUEEZE_TRACE();

    if (entry_header.content_size == 0) {
        SQUEEZE_ERROR("Symlink entry with no content");
        return "symlink entry with no content";
    }
    target.resize(static_cast<std::size_t>(entry_header.content_size) - 1);
    source.read(target.data(), target.size());
    if (source.fail())
        return "source reading failure";
    return success;
}

}
