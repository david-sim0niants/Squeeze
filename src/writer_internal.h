namespace squeeze {

struct Writer::FutureAppend {
    EntryInput& entry_input;
    Error<Writer> *error;

    FutureAppend(EntryInput& entry_input, Error<Writer> *error)
        : entry_input(entry_input), error(error)
    {}
};

struct Writer::FutureRemove {
    mutable std::string path;
    uint64_t pos, len;
    Error<Writer> *error;

    FutureRemove(std::string&& path, uint64_t pos, uint64_t len, Error<Writer> *error)
        : path(std::move(path)), pos(pos), len(len), error(error)
    {}
};

inline bool Writer::FutureRemoveCompare::operator()(const FutureRemove& a, const FutureRemove& b)
{
    return a.pos > b.pos;
}

}
