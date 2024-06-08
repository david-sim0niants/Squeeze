#pragma once

#include "reader.h"
#include "writer.h"
#include "entry.h"

namespace squeeze {

class Squeeze : public Reader, public Writer {
public:
    explicit Squeeze(std::iostream& stream)
        : Reader(stream), Writer(stream)
    {}

    void update();
};

}
