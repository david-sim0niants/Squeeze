#include "squeeze/writer.h"

#include "squeeze/logging.h"
#include "squeeze/utils/io.h"

namespace squeeze {

#undef SQUEEZE_LOG_FUNC_PREFIX
#define SQUEEZE_LOG_FUNC_PREFIX "squeeze::Writer::"

bool Writer::write()
{
    SQUEEZE_TRACE();
    bool succeeded = true;
    if (future_appends.empty()) {
        SQUEEZE_TRACE("No entry to append, performing removes synchronously");
        succeeded = perform_removes() && succeeded;
    } else {
        auto fut_succeeded = std::async(std::launch::async, [this](){ return perform_scheduled_writes(); });

        SQUEEZE_TRACE("Scheduling appends");
        succeeded = schedule_appends() && succeeded;

        SQUEEZE_TRACE("Waiting for scheduled writes to complete");
        succeeded = fut_succeeded.get() && succeeded;
    }

    SQUEEZE_TRACE("Stream put pointer at: {}", static_cast<long long>(Appender::target.tellp()));
    return succeeded;
}

inline bool Writer::perform_scheduled_writes()
{
    return perform_removes() && perform_scheduled_appends();
}

}
