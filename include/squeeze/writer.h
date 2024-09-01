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
    /* Construct the interface by passing a reference to the iostream target to operate on. */
    explicit Writer(std::iostream& target) : Appender(target), Remover(target)
    {
    }

    /* Perform all the registered entry append and remove operations.
     * The method guarantees that the put pointer of the target stream
     * will be at the new end of the stream.
     * Returns true if fully succeeded or false if some errors happened and may need to be checked. */
    bool write();

private:
    bool perform_scheduled_writes();
};

}
