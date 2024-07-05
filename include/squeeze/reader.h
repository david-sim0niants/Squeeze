#pragma once

#include "extracter.h"
#include "lister.h"

namespace squeeze {

/* Combines interfaces of Extracter and Lister */
class Reader : public Extracter, public Lister {
public:
    /* Construct the interface by passing a reference to the ostream source to read from. */
    explicit Reader(std::istream& source) : Extracter(source), Lister(source)
    {
    }
};

}
