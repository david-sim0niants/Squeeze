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

template<> std::string utils::stringify<EntryAttributes>(const EntryAttributes& attributes)
{
    std::string str = "?rwxrwxrwx";
    switch (attributes.type) {
    case EntryType::RegularFile:
        str[0] = '-';
        break;
    case EntryType::Directory:
        str[0] = 'd';
        break;
    case EntryType::Symlink:
        str[0] = 'l';
        break;
    default:
        break;
    }

    int i = 1;
    for (EntryPermissions perm : {
            EntryPermissions::OwnerRead,
            EntryPermissions::OwnerWrite,
            EntryPermissions::OwnerExec,
            EntryPermissions::GroupRead,
            EntryPermissions::GroupWrite,
            EntryPermissions::GroupExec,
            EntryPermissions::OthersRead,
            EntryPermissions::OthersWrite,
            EntryPermissions::OthersExec,
        }) {
        str[i] = test_flag(attributes.permissions, perm) ? str[i] : '-';
        ++i;
    }

    return str;
}

}
