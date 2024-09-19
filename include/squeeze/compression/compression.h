#pragma once

#include "params.h"
#include "config.h"
#include "huffman_15.h"
#include "deflate.h"

namespace squeeze::compression {

/* Additional compression-related flags. */
enum class CompressionFlags {
    None = 0,
    FinalBlock = 1, /* Compress as a final block of data. */
    ExpectFinalBlock = FinalBlock, /* Expect to compress the final block of data. */
};
SQUEEZE_DEFINE_ENUM_LOGIC_BITWISE_OPERATORS(CompressionFlags);

/* Stores an overall result of compression. */
struct CompressionResult {
    CompressionResult() = default;

    CompressionResult(Error<>&& error) : error(std::move(error))
    {
    }

    Error<> error = success;
};

/* Stores an overall result of decompression. */
struct DecompressionResult {
    enum Flags {
        None = 0,
        FinalBlock = 1, /* Indicates that the decompressed block is the final block of data. */
    };

    DecompressionResult() = default;

    explicit DecompressionResult(int flags) : flags(flags)
    {
    }

    explicit DecompressionResult(Error<>&& error) : error(std::move(error))
    {
    }

    DecompressionResult(int flags, Error<>&& error) : flags(flags), error(std::move(error))
    {
    }

    int flags = 0;
    Error<> error = success;
};

/* The compressor interface. */
template<std::output_iterator<char> OutIt, typename... OutItEnd>
    requires (sizeof...(OutItEnd) <= 1)
class Compressor {
public:
    using BitEncoder = misc::BitEncoder<char, CHAR_BIT, OutIt, OutItEnd...>;

    explicit Compressor(BitEncoder& bit_encoder) : bit_encoder(bit_encoder)
    {
    }

    /* Compress the given input data using the provided compression params and flags. */
    template<std::input_iterator InIt>
    std::tuple<InIt, CompressionResult> compress(InIt in_it, InIt in_it_end,
            CompressionParams params, CompressionFlags flags = CompressionFlags::None)
    {
        using enum CompressionMethod;
        switch (params.method) {
        case None:
            return compress<None>(in_it, in_it_end, params, flags);
        case Huffman:
            return compress<Huffman>(in_it, in_it_end, params, flags);
        case Deflate:
            return compress<Deflate>(in_it, in_it_end, params, flags);
        default:
            throw Exception<Compressor>("invalid compression method");
        }
    }

    /* Compress the given input data using the provided compression params and flags,
     * and compile-time provided compression method. */
    template<CompressionMethod method, std::input_iterator InIt>
    inline std::tuple<InIt, CompressionResult> compress(InIt in_it, InIt in_it_end,
            CompressionParams params, CompressionFlags flags = CompressionFlags::None)
    {
        using enum CompressionMethod;
        using CompressionFlags::FinalBlock;

        if constexpr (method == None)
            throw Exception<Compressor>("None compression not supported in this method");
        else if constexpr (method == Huffman)
            return compress_huffman(in_it, in_it_end, utils::test_flag(flags, FinalBlock));
        else if constexpr (method == Deflate)
            return compress_deflate(in_it, in_it_end, utils::test_flag(flags, FinalBlock), params.level);
        else
            throw Exception<Compressor>("invalid compression method");
    }

private:
    template<std::input_iterator InIt>
    std::tuple<InIt, CompressionResult> compress_huffman(InIt in_it, InIt in_it_end, bool final_block)
    {
        CompressionResult result;
        std::tie(in_it, result.error) = final_block ?
            huffman15_encode<true>(bit_encoder, in_it, in_it_end)
            :
            huffman15_encode<false>(bit_encoder, in_it, in_it_end)
        ;
        return std::make_tuple(in_it, std::exchange(result, CompressionResult()));
    }

    template<std::input_iterator InIt>
    std::tuple<InIt, CompressionResult> compress_deflate(InIt in_it, InIt in_it_end,
            bool final_block, uint8_t level)
    {
        using enum DeflateHeaderBits;
        DeflateParams deflate_params = get_deflate_params_for_level(level);
        deflate_params.header_bits = DynamicHuffman | utils::switch_flag(FinalBlock, final_block);
        return deflate(deflate_params, bit_encoder, in_it, in_it_end);
    }

    BitEncoder& bit_encoder;
};

/* The decompressor interface. */
template<std::input_iterator InIt, typename... InItEnd>
    requires (sizeof...(InItEnd) <= 1)
class Decompressor {
public:
    using BitDecoder = misc::BitDecoder<char, CHAR_BIT, InIt, InItEnd...>;

    explicit Decompressor(BitDecoder& bit_decoder) : bit_decoder(bit_decoder)
    {
    }

    /* Decompress to the provided output destination using the provided compression params and flags. */
    template<std::output_iterator<char> OutIt>
    std::tuple<OutIt, DecompressionResult> decompress(OutIt out_it, OutIt out_it_end,
            CompressionParams params, CompressionFlags flags)
    {
        using enum CompressionMethod;
        switch (params.method) {
        case None:
            return decompress<None>(out_it, out_it_end, params, flags);
        case Huffman:
            return decompress<Huffman>(out_it, out_it_end, params, flags);
        case Deflate:
            return decompress<Deflate>(out_it, out_it_end, params, flags);
        default:
            throw Exception<Decompressor>("invalid compression method");
        }
    }

    /* Decompress to the provided output destination using the provided compression params and flags,
     * and compile-time provided compression method. */
    template<CompressionMethod method, std::output_iterator<char> OutIt>
    std::tuple<OutIt, DecompressionResult> decompress(OutIt out_it, OutIt out_it_end,
            CompressionParams params, CompressionFlags flags)
    {
        using enum CompressionMethod;
        using CompressionFlags::ExpectFinalBlock;

        if constexpr (method == None)
            throw Exception<Decompressor>("None compression not supported in this method");
        else if constexpr (method == Huffman)
            return decompress_huffman(out_it, out_it_end, utils::test_flag(flags, ExpectFinalBlock));
        else if constexpr (method == Deflate)
            return decompress_deflate(out_it, out_it_end, params.level);
        else
            throw Exception<Decompressor>("invalid compression method");
    }

private:
    template<std::output_iterator<char> OutIt>
    std::tuple<OutIt, DecompressionResult> decompress_huffman(OutIt out_it, OutIt out_it_end,
            bool expect_final_block)
    {
        DecompressionResult result;
        std::tie(out_it, result.error) = expect_final_block ?
            huffman15_decode<true>(out_it, out_it_end, bit_decoder)
            :
            huffman15_decode<false>(out_it, out_it_end, bit_decoder)
        ;
        int flags = utils::switch_flag(DecompressionResult::FinalBlock, expect_final_block);
        return std::make_tuple(out_it, std::exchange(result, DecompressionResult()));
    }

    template<std::output_iterator<char> OutIt>
    std::tuple<OutIt, DecompressionResult> decompress_deflate(OutIt out_it, OutIt out_it_end, uint8_t level)
    {
        DecompressionResult result;
        DeflateHeaderBits header_bits {};
        std::tie(out_it, header_bits, result.error) = inflate(out_it, out_it_end, bit_decoder);
        const bool final_block = utils::test_flag(header_bits, DeflateHeaderBits::FinalBlock);
        result.flags = utils::switch_flag(DecompressionResult::FinalBlock, final_block);
        return std::make_tuple(out_it, std::exchange(result, DecompressionResult()));
    }

    BitDecoder& bit_decoder;
};

/* Compress data from the given input data to the given bit encoder using
 * the provided compression params and flags.
 * Return an input iterator at the point where compression stopped and the compression result. */
template<std::input_iterator InIt, std::output_iterator<char> OutIt, typename... OutItEnd>
    requires (sizeof...(OutItEnd) <= 1)
std::tuple<InIt, CompressionResult> compress(InIt in_it, InIt in_it_end,
        CompressionParams params, CompressionFlags flags,
        misc::BitEncoder<char, CHAR_BIT, OutIt, OutItEnd...>& bit_encoder)
{
    return Compressor(bit_encoder).encode(in_it, in_it_end, params, flags);
}

/* Decompress data from the given bit decoder to the given output destination using
 * the provided compression params and flags.
 * Return an output iterator at the point where decompression stopped and the decompression result. */
template<std::output_iterator<char> OutIt, std::input_iterator InIt, typename ...InItEnd>
    requires (sizeof...(InItEnd) <= 1)
std::tuple<OutIt, DecompressionResult> decompress(OutIt out_it, OutIt out_it_end,
        CompressionParams params, CompressionFlags flags,
        misc::BitDecoder<char, CHAR_BIT, InIt, InItEnd...>& bit_decoder)
{
    return Decompressor(bit_decoder).decode(out_it, out_it_end, params, flags);
}

/* Compress data from the given input data to the given output destination using
 * the provided compression params and flags.
 * Return input and output iterators at the point where compression stopped and the compression result. */
template<std::input_iterator InIt, std::output_iterator<char> OutIt, typename ...OutItEnd>
    requires (sizeof...(OutItEnd) <= 1)
std::tuple<InIt, OutIt, CompressionResult> compress(InIt in_it, InIt in_it_end,
        CompressionParams params, CompressionFlags flags,
        OutIt out_it, OutItEnd... out_it_end)
{
    auto bit_encoder = misc::make_bit_encoder(out_it, out_it_end...);
    CompressionResult result;
    std::tie(in_it, result) = Compressor(bit_encoder).compress(in_it, in_it_end, params, flags);
    bit_encoder.finalize();
    return std::make_tuple(in_it, out_it, std::exchange(result, CompressionResult()));
}

/* Decompress data from the given input data to the given output destination using
 * the provided compression params and flags.
 * Return input and output iterators at the point where decompression stopped and the decompression result. */
template<std::output_iterator<char> OutIt, std::input_iterator InIt, typename ...InItEnd>
    requires (sizeof...(InItEnd) <= 1)
std::tuple<OutIt, InIt, DecompressionResult> decompress(OutIt out_it, OutIt out_it_end,
        CompressionParams params, CompressionFlags flags,
        InIt in_it, InItEnd... in_it_end)
{
    auto bit_decoder = misc::make_bit_decoder(in_it, in_it_end...);
    DecompressionResult result;
    std::tie(out_it, result) = Decompressor(bit_decoder).decompress(out_it, out_it_end, params, flags);
    return std::make_tuple(out_it, in_it, std::exchange(result, DecompressionResult()));
}

}
