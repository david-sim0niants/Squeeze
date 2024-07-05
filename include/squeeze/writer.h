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
     * will be at the new end of the stream */
    void write();
};

}
