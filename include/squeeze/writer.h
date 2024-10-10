#pragma once

#include <memory>
#include <thread>

#include "entry_iterator.h"
#include "appender.h"
#include "remover.h"

namespace squeeze {

/* Combines interfaces of Appender and Remover */
class Writer : public Appender, public Remover {
public:
    using Stat = Appender::Stat;
    static_assert(std::is_same_v<Appender::Stat, Remover::Stat>, "mismatch of the writer status types");

    /* Construct the interface by passing a reference to the iostream target to operate on. */
    explicit Writer(std::iostream& target) : Appender(target), Remover(target)
    {
    }

    /* Perform all the registered entry append and remove operations.
     * The method guarantees that the put pointer of the target stream
     * will be at the new end of the stream.
     * Returns true if fully successful, or false if errors occurred and may need further checking. */
    bool write();

private:
    bool perform_scheduled_writes();
};

}
