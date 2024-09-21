#include "squeeze/entry_common.h"

namespace squeeze {

template<> void print_to(std::ostream& os, const EntryAttributes& attributes)
{
    char str[] = "?rwxrwxrwx";
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
        str[i] = utils::test_flag(attributes.permissions, perm) ? str[i] : '-';
        ++i;
    }

    os << str;
}

}
