// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include "reader.h"
#include "writer.h"
#include "entry.h"

namespace squeeze {

/** The main Squeeze interface, combining interfaces of both Reader and Writer.
 * Also provides an additional update() method as an alternative for the write()
 * method inherited from Writer that handles cases with entries
 * already existing in the squeeze that can be updated. */
class Squeeze : public Reader, public Writer {
public:
    explicit Squeeze(std::iostream& stream) : Reader(stream), Writer(stream)
    {
    }

    /** The update method functions similarly to write(), but it handles cases where append
     * operations are registered for entries that already exist with the same path in the stream.
     * It ensures that these existing entries are removed before being re-appended,
     * effectively updating the entries.
     * The method guarantees that the put pointer of the target stream
     * will be at the new end of the stream.
     * Returns true if fully successful, or false if errors occurred and may need further checking. */
    bool update();
};

}
