#include "squeeze/writer.h"

#include "squeeze/logging.h"
#include "squeeze/utils/io.h"

namespace squeeze {

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::Writer::"

void Writer::write()
{
    SQUEEZE_TRACE();
    if (future_appends.empty()) {
        SQUEEZE_TRACE("No entry to append, performing removes synchronously");
        perform_removes();
    } else {
        auto future_void = std::async(std::launch::async,
            [this]
            {
                perform_removes();
                perform_scheduled_appends();
            }
        );
        SQUEEZE_TRACE("Waiting for scheduled append tasks to complete");
        schedule_appends();
        future_void.get();
    }
    SQUEEZE_TRACE("Stream put pointer at: {}", static_cast<long long>(Appender::target.tellp()));
}

}
