#pragma once

#include <unordered_map>
#include <sstream>
#include <variant>
#include <functional>
#include <memory>

#include "squeeze/entry.h"

namespace squeeze::testing::tools {

class MockAbstractFile {
public:
    explicit MockAbstractFile(EntryPermissions permissions = default_permissions)
        : MockAbstractFile(EntryAttributes{EntryType::None, permissions})
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
    explicit MockAbstractFile(EntryAttributes attributes)
        : attributes(attributes)
    {}

    static constexpr EntryPermissions default_permissions {
        EntryPermissions::OwnerRead | EntryPermissions::OwnerWrite |
        EntryPermissions::GroupRead | EntryPermissions::GroupWrite |
        EntryPermissions::OthersRead | EntryPermissions::OthersWrite,
    };

    EntryAttributes attributes;
};


class MockRegularFile final : public MockAbstractFile {
public:
    template<typename ...Args>
    explicit MockRegularFile(EntryPermissions permissions,
            std::stringstream&& contents = std::stringstream())
        : MockAbstractFile(EntryAttributes{EntryType::RegularFile, permissions}),
        contents(std::move(contents))
    {}

    template<typename ...Args>
    explicit MockRegularFile(std::stringstream&& contents = std::stringstream())
        : MockRegularFile(default_permissions, std::move(contents))
    {}

    MockRegularFile(const MockRegularFile& other)
        : MockAbstractFile(other), contents(other.contents.str())
    {}

    MockRegularFile& operator=(const MockRegularFile& other)
    {
        if (this == &other)
            return *this;
        MockAbstractFile::operator=(other);
        std::stringstream copied(other.contents.str());
        contents.swap(copied);
        return *this;
    }

    std::stringstream contents;
};


class MockSymlink : public MockAbstractFile {
public:
    explicit MockSymlink(EntryPermissions permissions, std::string&& target = {})
        : MockAbstractFile(EntryAttributes{EntryType::Symlink, permissions}), target(std::move(target))
    {}

    explicit MockSymlink(std::string&& target = {})
        : MockSymlink(default_permissions, std::move(target))
    {}

    std::string target;
};


class MockDirectory;

template<typename MockFile>
using MockFileEntriesMap = std::unordered_map<std::string, MockFile>;

struct MockFileEntries {
    MockFileEntriesMap<std::shared_ptr<MockRegularFile>> regular_files;
    MockFileEntriesMap<std::shared_ptr<MockSymlink>> symlinks;
    MockFileEntriesMap<std::shared_ptr<MockDirectory>> directories;

    template<typename MockFile> auto& get()
    {
        if constexpr (std::is_same_v<MockFile, MockRegularFile>)
            return regular_files;
        else if constexpr (std::is_same_v<MockFile, MockSymlink>)
            return symlinks;
        else
            return directories;
    }

    template<typename MockFile> const auto& get() const
    {
        if constexpr (std::is_same_v<MockFile, MockRegularFile>)
            return regular_files;
        else if constexpr (std::is_same_v<MockFile, MockSymlink>)
            return symlinks;
        else
            return directories;
    }
};


class MockDirectory : public MockAbstractFile {
public:
    explicit MockDirectory(EntryPermissions permissions, MockFileEntries&& entries = {})
        : MockAbstractFile(EntryAttributes{EntryType::Directory, permissions}), entries(entries)
    {}

    explicit MockDirectory(MockFileEntries&& entries = {})
        : MockDirectory(default_permissions, std::move(entries))
    {}

    template<typename MockFile, typename F>
    void list(F on_file) const
    {
        for (auto& [name, entry] : entries.get<MockFile>())
            on_file(name, entry);
    }

    template<typename MockFile, typename F>
    void list(F on_file, std::string& path) const
    {
        for (auto& [name, entry] : entries.get<MockFile>()) {
            path.append(name);
            on_file(path, entry);
            path.erase(path.size() - name.size(), name.size());
        }
    }

    template<typename ...MockFiles, typename ...OnFiles>
    void list_recursively(OnFiles... on_files) const
    {
        std::string path;
        list_recursively<MockFiles...>(path, on_files...);
    }

    template<typename ...MockFiles, typename ...OnFiles>
    void list_recursively(std::string& path, OnFiles... on_files) const
    {
        (..., ListHelp<MockFiles, OnFiles>{*this}.template list<MockFiles...>(on_files, path, on_files...));
        if constexpr (!(std::is_same_v<MockDirectory, MockFiles> || ...))
            list<MockDirectory>(
                [&on_files...](std::string& path, const std::shared_ptr<MockDirectory> directory)
                {
                    directory->list_recursively<MockFiles...>(path, on_files...);
                }, path);
    }

    std::shared_ptr<MockDirectory> make_directories(std::string_view path);

    template<typename MockFile, typename ...Args>
    std::shared_ptr<MockFile> make(std::string_view path, Args&&... args)
    {
        return MakeHelp<MockFile>{*this}.make(path, std::forward<Args>(args)...);
    }

    void update(const MockDirectory& another)
    {
        auto make_file = [this](const std::string& path, auto file)
        {
            *make<typename decltype(file)::element_type>(path) = *file;
        };

        another.list_recursively<MockRegularFile, MockDirectory, MockSymlink>(
                make_file, make_file, make_file);
    }

    static constexpr char seperator = '/';
    MockFileEntries entries;

private:
    template<typename MockFile, typename OnFile>
    struct ListHelp {
        const MockDirectory& owner;

        template<typename ...MockFiles, typename ...OnFiles>
        inline void list(OnFile on_file, std::string& path, OnFiles&...) const
        {
            owner.list<MockFile>(on_file, path);
        }
    };

    template<typename OnDirectory>
    struct ListHelp<MockDirectory, OnDirectory> {
        const MockDirectory& owner;

        template<typename ...MockFiles, typename ...OnFiles>
        inline void list(OnDirectory on_directory, std::string& path, OnFiles&... on_files) const
        {
            owner.list<MockDirectory>(
                [&on_directory, &on_files...]
                (std::string& path, const std::shared_ptr<MockDirectory> directory)
                {
                    on_directory(path, directory);
                    directory->list_recursively<MockFiles...>(path, on_files...);
                }, path
            );
        }
    };

    template<typename MockFile, typename = void>
    struct MakeHelp {
        MockDirectory& owner;

        template<typename ...Args>
        std::shared_ptr<MockFile> make(std::string_view path, Args&&... args)
        {
            size_t i_sep = path.find_last_of(seperator);
            MockDirectory *lastdir = &owner;
            if (i_sep != std::string_view::npos)
                lastdir = owner.make_directories(path.substr(0, i_sep + 1)).get();
            return lastdir->entries.get<MockFile>()[std::string(path.substr(i_sep + 1))] =
                    std::make_shared<MockFile>(std::forward<Args>(args)...);
        }
    };

    template<typename Explicit_Specialization_Preventing_Type>
    struct MakeHelp<MockDirectory, Explicit_Specialization_Preventing_Type> {
        MockDirectory& owner;

        template<typename ...Args>
        inline std::shared_ptr<MockDirectory> make(std::string_view path, Args&&...)
        {
            return owner.make_directories(path);
        }
    };
};


using MockFileSystem = MockDirectory; // kinda really does act like a directory

MockFileEntries flatten_mockfs(const MockFileSystem& mockfs);

}
