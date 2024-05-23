#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <type_traits>

#include "compression_method.h"
#include "reader.h"
#include "entry.h"

namespace squeeze {

class Writer {
private:
    struct FutureAppend;
    struct FutureRemove;
    struct FutureRemoveCompare {
        bool operator()(const FutureRemove& a, const FutureRemove& b);
    };

public:
    explicit Writer(std::iostream& target);
    ~Writer();

    template<typename T, typename ...Args>
    inline void will_append(Args&&... args) requires std::is_base_of_v<EntryType, T>
    {
        will_append(std::make_unique<T>(std::forward<Args>(args)...));
    }

    template<typename T, typename ...Args>
    inline void will_append(Error<Writer>& err, Args&&... args) requires std::is_base_of_v<EntryType, T>
    {
        will_append(std::make_unique<T>(std::forward<Args>(args)...), &err);
    }

    void will_append(std::unique_ptr<EntryInput>&& entry_input, Error<Writer> *err = nullptr);
    void will_append(EntryInput& entry_input, Error<Writer> *err = nullptr);
    void will_remove(const ReaderIterator& it, Error<Writer> *err = nullptr);

    void write();

    template<typename T, typename ...Args>
    inline Error<Writer> append(Args&&... args) requires std::is_base_of_v<EntryType, T>
    {
        T entry_input(std::forward<Args>(args)...);
        return append(entry_input);
    }

    Error<Writer> append(std::unique_ptr<EntryInput>&& entry_input);
    Error<Writer> append(EntryInput& entry_input);
    Error<Writer> remove(const ReaderIterator&& it);

private:
    void perform_removes();
    void perform_appends();
    Error<Writer> perform_append(EntryInput& insert_request);

    std::iostream& target;
    Reader reader;
    std::vector<std::unique_ptr<EntryInput>> owned_entry_inputs;
    std::vector<FutureAppend> future_appends;
    std::priority_queue<FutureRemove, std::vector<FutureRemove>, FutureRemoveCompare> future_removes;
};

}
