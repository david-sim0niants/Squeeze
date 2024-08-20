#include "squeeze/compression/deflate_huffman.h"

namespace squeeze::compression {

template class DeflateHuffman<BasicHuffmanPolicy<15>, BasicHuffmanPolicy<7>>;

}
