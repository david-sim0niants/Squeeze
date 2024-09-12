#pragma once

#include <unordered_map>
#include <sstream>
#include <variant>
#include <functional>
#include <memory>

#include "squeeze/entry.h"

namespace squeeze::test_tools::mock {

class AbstractFile {
public:
    explicit AbstractFile(EntryPermissions permissions = default_permissions)
        : AbstractFile(EntryAttributes{EntryType::None, permissions})
    {}

    inline EntryAttributes get_attributes() const
    {
        return attributes;
    }

    inline void set_permissions(EntryPermissions permissions)
    {
        attributes.permissions = permissions;
    }

protected:
    explicit AbstractFile(EntryAttributes attributes)
        : attributes(attributes)
    {}

    static constexpr EntryPermissions default_permissions {
        EntryPermissions::OwnerRead | EntryPermissions::OwnerWrite |
        EntryPermissions::GroupRead | EntryPermissions::GroupWrite |
        EntryPermissions::OthersRead | EntryPermissions::OthersWrite,
    };

    EntryAttributes attributes;
};


class RegularFile final : public AbstractFile {
public:
    template<typename ...Args>
    explicit RegularFile(EntryPermissions permissions,
            std::stringstream&& contents = std::stringstream())
        : AbstractFile(EntryAttributes{EntryType::RegularFile, permissions}),
        contents(std::move(contents))
    {}

    template<typename ...Args>
    explicit RegularFile(std::stringstream&& contents = std::stringstream())
        : RegularFile(default_permissions, std::move(contents))
    {}

    RegularFile(const RegularFile& other)
        : AbstractFile(other), contents(other.contents.str())
    {
    }

    RegularFile& operator=(const RegularFile& other)
    {
        if (this == &other)
            return *this;
        AbstractFile::operator=(other);
        contents = std::stringstream(other.contents.str());
        return *this;
    }

    std::stringstream contents;
};


class Symlink : public AbstractFile {
public:
    explicit Symlink(EntryPermissions permissions, std::string&& target = {})
        : AbstractFile(EntryAttributes{EntryType::Symlink, permissions}), target(std::move(target))
    {}

    explicit Symlink(std::string&& target = {})
        : Symlink(default_permissions, std::move(target))
    {}

    std::string target;
};


class Directory;

template<typename File>
using FileEntriesMap = std::unordered_map<std::string, File>;

struct FileEntries {
    FileEntriesMap<std::shared_ptr<RegularFile>> regular_files;
    FileEntriesMap<std::shared_ptr<Symlink>> symlinks;
    FileEntriesMap<std::shared_ptr<Directory>> directories;

    template<typename File> auto& get()
    {
        if constexpr (std::is_same_v<File, RegularFile>)
            return regular_files;
        else if constexpr (std::is_same_v<File, Symlink>)
            return symlinks;
        else
            return directories;
    }

    template<typename File> const auto& get() const
    {
        if constexpr (std::is_same_v<File, RegularFile>)
            return regular_files;
        else if constexpr (std::is_same_v<File, Symlink>)
            return symlinks;
        else
            return directories;
    }
};


class Directory : public AbstractFile {
public:
    explicit Directory(EntryPermissions permissions, FileEntries&& entries = {})
        : AbstractFile(EntryAttributes{EntryType::Directory, permissions}), entries(entries)
    {}

    explicit Directory(FileEntries&& entries = {})
        : Directory(default_permissions, std::move(entries))
    {}

    template<typename File, typename F>
    void list(F on_file) const
    {
        for (auto& [name, entry] : entries.get<File>())
            on_file(name, entry);
    }

    template<typename File, typename F>
    void list(F on_file, std::string& path) const
    {
        for (auto& [name, entry] : entries.get<File>()) {
            path.append(name);
            on_file(path, entry);
            path.erase(path.size() - name.size(), name.size());
        }
    }

    template<typename ...Files, typename ...OnFiles>
    void list_recursively(OnFiles... on_files) const
    {
        std::string path;
        list_recursively<Files...>(path, on_files...);
    }

    template<typename ...Files, typename ...OnFiles>
    void list_recursively(std::string& path, OnFiles... on_files) const
    {
        (..., ListHelp<Files, OnFiles>{*this}.template list<Files...>(on_files, path, on_files...));
        if constexpr (!(std::is_same_v<Directory, Files> || ...))
            list<Directory>(
                [&on_files...](std::string& path, const std::shared_ptr<Directory> directory)
                {
                    directory->list_recursively<Files...>(path, on_files...);
                }, path);
    }

    std::shared_ptr<Directory> make_directories(std::string_view path);

    template<typename File, typename ...Args>
    std::shared_ptr<File> make(std::string_view path, Args&&... args)
    {
        return MakeHelp<File>{*this}.make(path, std::forward<Args>(args)...);
    }

    void update(const Directory& another)
    {
        auto make_file = [this](const std::string& path, auto file)
        {
            *make<typename decltype(file)::element_type>(path) = *file;
        };

        another.list_recursively<RegularFile, Directory, Symlink>(
                make_file, make_file, make_file);
    }

    static constexpr char seperator = '/';
    FileEntries entries;

private:
    template<typename File, typename OnFile>
    struct ListHelp {
        const Directory& owner;

        template<typename ...Files, typename ...OnFiles>
        inline void list(OnFile on_file, std::string& path, OnFiles&...) const
        {
            owner.list<File>(on_file, path);
        }
    };

    template<typename OnDirectory>
    struct ListHelp<Directory, OnDirectory> {
        const Directory& owner;

        template<typename ...Files, typename ...OnFiles>
        inline void list(OnDirectory on_directory, std::string& path, OnFiles&... on_files) const
        {
            owner.list<Directory>(
                [&on_directory, &on_files...]
                (std::string& path, const std::shared_ptr<Directory> directory)
                {
                    on_directory(path, directory);
                    directory->list_recursively<Files...>(path, on_files...);
                }, path
            );
        }
    };

    template<typename File, typename = void>
    struct MakeHelp {
        Directory& owner;

        template<typename ...Args>
        std::shared_ptr<File> make(std::string_view path, Args&&... args)
        {
            size_t i_sep = path.find_last_of(seperator);
            Directory *lastdir = &owner;
            if (i_sep != std::string_view::npos)
                lastdir = owner.make_directories(path.substr(0, i_sep + 1)).get();
            return lastdir->entries.get<File>()[std::string(path.substr(i_sep + 1))] =
                    std::make_shared<File>(std::forward<Args>(args)...);
        }
    };

    template<typename Explicit_Specialization_Preventing_Type>
    struct MakeHelp<Directory, Explicit_Specialization_Preventing_Type> {
        Directory& owner;

        template<typename ...Args>
        inline std::shared_ptr<Directory> make(std::string_view path, Args&&...)
        {
            return owner.make_directories(path);
        }
    };
};


using FileSystem = Directory; // kinda really does act like a directory

FileEntries flatten_fs(const FileSystem& fs);

}
