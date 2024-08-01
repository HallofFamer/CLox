#include <stdlib.h>
#include <string.h>
#include "buffer.h"

#pragma warning disable 6308
DEFINE_BUFFER(ByteArray, uint8_t)
DEFINE_BUFFER(IntArray, int)
DEFINE_BUFFER(DoubleArray, double) 
#pragma warning restore 6308