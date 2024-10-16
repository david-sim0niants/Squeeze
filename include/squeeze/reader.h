// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include "extracter.h"
#include "lister.h"

namespace squeeze {

/** Combines interfaces of Extracter and Lister */
class Reader : public Extracter, public Lister {
public:
    using Stat = Extracter::Stat;

    /** Construct the interface by passing a reference to the ostream source to read from. */
    explicit Reader(std::istream& source) : Extracter(source), Lister(source)
    {
    }
};

}
