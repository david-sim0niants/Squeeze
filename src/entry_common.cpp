#include "squeeze/entry_common.h"

namespace squeeze {

std::istream& operator>>(std::istream& input, EntryAttributes& entry_attributes)
{
    input.read(reinterpret_cast<char *>(&entry_attributes), sizeof(EntryAttributes));
    return input;
}

std::ostream& operator<<(std::ostream& output, const EntryAttributes& entry_attributes)
{
    output.write(reinterpret_cast<const char *>(&entry_attributes), sizeof(EntryAttributes));
    return output;
}

}
