#pragma once

#include <iostream>
#include <iterator>

namespace squeeze::utils {

void iosmove(std::iostream& ios, std::streampos dst, std::streampos src, std::streamsize len);
void ioscopy(std::istream& src_stream, std::streampos src_pos,
             std::ostream& dst_stream, std::streampos dst_pos,
             std::streamsize cpy_len);

}
