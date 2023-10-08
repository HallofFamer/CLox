#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "shape.h"

void initShapeTree(ShapeTree* shapeTree) {
    shapeTree->list = NULL;
    shapeTree->count = 0;
    shapeTree->rootShape = NULL;
}

void freeShapeTree(VM* vm, ShapeTree* shapeTree) {
    FREE_ARRAY(Shape, shapeTree->list, shapeTree->capacity);
    initShapeTree(shapeTree);
}