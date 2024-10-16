// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#include "test_tools/printing.h"

namespace squeeze::test_tools {

void print_byte(char byte, std::ostream& os)
{
    os << std::hex;
    os << "('";

    switch (byte) {
    case '\n':
        os << "\\n";
        break;
    case '\t':
        os << "\\t";
        break;
    case '\r':
        os << "\\r";
        break;
    case '\0':
        os << "\\0";
        break;
    default:
        if (std::isprint(static_cast<unsigned char>(byte)))
            os << byte;
        else
            os << "\\x" << ((unsigned short)(byte) & 0xFF);
        break;
    }

    os << "', 0x" << ((unsigned short)(byte) & 0xFF) << ')';
    os << std::dec;
}

void print_data(const char *data, std::size_t size, std::ostream& os, std::size_t print_size_limit)
{
    os << "(size=" << size << ", data={";
    const std::size_t output_len = std::min(size, print_size_limit);
    for (std::size_t i = 0; i < output_len; ++i) {
        print_byte(data[i], os);
        if (i + 1 < output_len)
            os << ", ";
    }
    os << (output_len < size ? ", ...}" : "}") << ')';
}

}
