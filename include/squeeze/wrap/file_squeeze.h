#pragma once

#include "squeeze/squeeze.h"
#include "file_appender.h"
#include "file_remover.h"
#include "file_extracter.h"

namespace squeeze::wrap {

class FileSqueeze : public FileAppender, public FileRemover, public FileExtracter {
public:
    explicit FileSqueeze(Squeeze& squeeze)
        : squeeze(squeeze), FileAppender(squeeze), FileRemover(squeeze), FileExtracter(squeeze)
    {}

    void update();

    inline auto& get_wrappee()
    {
        return squeeze;
    }

private:
    Squeeze& squeeze;
};

}
