// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#include <istream>

#include "common.h"
#include "status.h"
#include "compression/params.h"

namespace squeeze {

using compression::CompressionParams;
using EncodeStat = StatStr;

/** Encode single buffer using the compression info provided. */
EncodeStat encode_buffer(const Buffer& in, Buffer& out, const CompressionParams& compression);

/** Encode a char stream of a given size into another char stream using the compression info provided.
 * Unlike the EncoderPool, doesn't use multithreading. */
EncodeStat encode(std::istream& in, std::size_t size, std::ostream& out,
               const compression::CompressionParams& compression);

}
