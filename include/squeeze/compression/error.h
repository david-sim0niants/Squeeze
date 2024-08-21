#pragma once

#include "squeeze/error.h"
#include "squeeze/utils/stringify.h"

namespace squeeze::compression {

struct EncodeErrorMessage {
    std::string error;
    std::size_t nr_bits_unencoded {};
};

struct DecodeErrorMessage {
    std::string error;
    std::size_t nr_bits_undecoded {};
};

template<typename RelatedType = void>
using EncodeError = Error<RelatedType, EncodeErrorMessage>;

template<typename RelatedType = void>
using DecodeError = Error<RelatedType, DecodeErrorMessage>;

}

namespace squeeze::utils {

template<> inline std::string stringify(const compression::EncodeErrorMessage& msg)
{
    return msg.error + " (" + std::to_string(msg.nr_bits_unencoded) + " unencoded bits)";
}

template<> inline std::string stringify(const compression::DecodeErrorMessage& msg)
{
    return msg.error + " (" + std::to_string(msg.nr_bits_undecoded) + " undecoded bits)";
}

}
