#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"

DEFINE_BUFFER(BoolArray, bool)
DEFINE_BUFFER(ByteArray, uint8_t)
DEFINE_BUFFER(ShortArray, short)
DEFINE_BUFFER(IntArray, int)
DEFINE_BUFFER(LongArray, long)
DEFINE_BUFFER(FloatArray, float)
DEFINE_BUFFER(DoubleArray, double)
DEFINE_BUFFER(CharArray, char)
DEFINE_BUFFER(StringArray, char*)