#pragma once

#include "reader.h"
#include "writer.h"
#include "entry.h"

namespace squeeze {

/* The main Squeeze interface, combining functionality of both the Reader and Writer.
 * Also provides an additional update() method as an enhancement
 * of the write() method inherited from Writer. */
class Squeeze : public Reader, public Writer {
public:
    explicit Squeeze(std::iostream& stream)
        : Reader(stream), Writer(stream)
    {}

    /* The update method functions similarly to write(), but it handles cases where append
     * operations are registered for entries that already exist with the same path in the stream.
     * It ensures that these existing entries are removed before being re-appended,
     * effectively updating the entries. */
    void update();
};

}
