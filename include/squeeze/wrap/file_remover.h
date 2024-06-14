#pragma once

#include "squeeze/squeeze.h"
#include "squeeze/utils/fs.h"

#include <functional>

namespace squeeze::wrap {

/* Wrapper over the Squeeze interface for providing additional remove methods
 * specifically designed for handling files. */
class FileRemover {
public:
    explicit FileRemover(Squeeze& squeeze) : squeeze(squeeze)
    {}

    /* Find an entry by its path and remove it. */
    bool will_remove(const std::string_view path, Error<Writer> *err = nullptr);

    /* Remove recursively all entries within the path.
     * get_err_ptr() is supposed to provide a pointer to the subsequent error. */
    bool will_remove_recursively(const std::string_view path,
            const std::function<Error<Writer> *()>& get_err_ptr = [](){ return nullptr; });

    /* Remove all entries.
     * get_err_ptr() is supposed to provide a pointer to the subsequent error. */
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
