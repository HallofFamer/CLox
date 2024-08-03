#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"

DEFINE_BUFFER(ByteArray, uint8_t)
DEFINE_BUFFER(IntArray, int)
DEFINE_BUFFER(DoubleArray, double)
DEFINE_BUFFER(StringArray, char*)