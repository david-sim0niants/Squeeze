#pragma once

#include "squeeze/squeeze.h"
#include "squeeze/utils/fs.h"

#include <functional>

namespace squeeze::wrap {

class FileRemover {
public:
    explicit FileRemover(Squeeze& squeeze) : squeeze(squeeze)
    {}

    bool will_remove(const std::string_view path, Error<Writer> *err = nullptr);

    bool will_remove_recursively(const std::string_view path,
            const std::function<Error<Writer> *()>& get_err_ptr = [](){ return nullptr; });

    void will_remove_all(
            const std::function<Error<Writer> *()>& get_err_ptr = [](){ return nullptr; });

    inline auto& get_wrappee()
    {
        return squeeze;
    }

private:
    Squeeze& squeeze;
};

}
