#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "shape.h"

static void initRootShape(Shape* shape) {
    shape->id = 0;
    shape->parentID = 0;
    shape->type = SHAPE_ROOT;
    shape->nextIndex = 0;
    initTable(&shape->edges);
    initTable(&shape->indexes);
}

void initShapeTree(VM* vm) {
    ShapeTree shapeTree;
    shapeTree.list = NULL;
    shapeTree.count = 0;
    shapeTree.capacity = 0;
    shapeTree.rootShape = NULL;
    vm->shapes = shapeTree;

    Shape rootShape;
    initRootShape(&rootShape);
    shapeTreeAppend(vm, rootShape);
}

void freeShapeTree(VM* vm, ShapeTree* shapeTree) {
    FREE_ARRAY(Shape, shapeTree->list, shapeTree->capacity);
    shapeTree->list = NULL;
    shapeTree->count = 0;
    shapeTree->capacity = 0;
    shapeTree->rootShape = NULL;
}

void shapeTreeAppend(VM* vm, Shape shape) {
    ShapeTree* shapeTree = &vm->shapes;
    if (shapeTree->capacity < shapeTree->count + 1) {
        int oldCapacity = shapeTree->capacity;
        shapeTree->capacity = GROW_CAPACITY(oldCapacity);
        shapeTree->list = GROW_ARRAY(Shape, shapeTree->list, oldCapacity, shapeTree->capacity);
    }

    shapeTree->list[shapeTree->count] = shape;
    shapeTree->count++;
}