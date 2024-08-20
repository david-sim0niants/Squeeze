#include "squeeze/compression/huffman.h"

namespace squeeze::compression {

template class Huffman<BasicHuffmanPolicy<15>>;
template class Huffman<BasicHuffmanPolicy<7>>;

}
