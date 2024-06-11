#pragma once

#include "squeeze/squeeze.h"
#include "squeeze/utils/fs.h"

#include <iterator>

namespace squeeze::wrap {

class FileRemover {
public:
    explicit FileRemover(Squeeze& squeeze) : squeeze(squeeze)
    {}

    bool will_remove(const std::string_view path, Error<Writer> *err = nullptr);

    template<std::output_iterator<Error<Writer>> ErrIt>
    void will_remove_recursively(const std::string_view path, ErrIt err_it, ErrIt err_end)
    {
        for (auto it = squeeze.begin(); it != squeeze.end(); ++it) {
            if (!utils::path_within_dir(it->second.path, path))
                continue;
            if (err_it == err_end) [[unlikely]]
                break;
            squeeze.will_remove(it, &*err_it);
            ++err_it;
        }
    }

    void will_remove_recursively(const std::string_view path);

    inline auto& get_wrappee()
    {
        return squeeze;
    }

private:
    Squeeze& squeeze;
};

}
