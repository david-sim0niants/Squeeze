#pragma once

#include <iostream>

namespace squeeze::utils {

void iosmove(std::iostream& ios, std::streampos dst, std::streampos src, std::streamsize len);

}
