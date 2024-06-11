#include "squeeze/wrap/file_extracter.h"

namespace squeeze::wrap {

void FileExtracter::extract_recursively(const std::string_view path)
{
    for (auto it = reader.begin(); it != reader.end(); ++it) {
        if (!utils::path_within_dir(it->second.path, path))
            break;
        reader.extract(it);
    }
}

}
