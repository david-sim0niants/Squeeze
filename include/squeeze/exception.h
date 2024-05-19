#pragma once

#include <stdexcept>

namespace squeeze {

struct BaseException : std::runtime_error {
    BaseException(const std::string& msg) : std::runtime_error("Error: " + msg + '.') {}
    BaseException(const std::string& msg, int code) :
        std::runtime_error("Error: " + msg + ". (" + std::to_string(code) + ')')
    {}
};

template<typename RelatedType>
struct Exception : BaseException {
    Exception(const std::string& msg) : BaseException(msg) {}
    Exception(const std::string& msg, int code) : BaseException(msg, code) {}
};

}
