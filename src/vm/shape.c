#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "shape.h"

static void initRootShape(Shape* shape) {
    shape->id = 0;
    shape->parentID = 0;
    shape->type = SHAPE_ROOT;
    shape->nextIndex = 0;
    initIndexMap(&shape->edges);
    initIndexMap(&shape->indexes);
}

void initShapeTree(VM* vm) {
    ShapeTree shapeTree = { .list = NULL, .count = 0, .capacity = 0, .rootShape = NULL };
    vm->shapes = shapeTree;

    Shape rootShape;
    initRootShape(&rootShape);
    appendToShapeTree(vm, &rootShape);
}

void freeShapeTree(VM* vm, ShapeTree* shapeTree) {
    FREE_ARRAY(Shape, shapeTree->list, shapeTree->capacity);
    shapeTree->list = NULL;
    shapeTree->count = 0;
    shapeTree->capacity = 0;
    shapeTree->rootShape = NULL;
}

void appendToShapeTree(VM* vm, Shape* shape) {
    ShapeTree* shapeTree = &vm->shapes;
    if (shapeTree->capacity < shapeTree->count + 1) {
        int oldCapacity = shapeTree->capacity;
        shapeTree->capacity = GROW_CAPACITY(oldCapacity);
        shapeTree->list = GROW_ARRAY(Shape, shapeTree->list, oldCapacity, shapeTree->capacity);
    }

    shapeTree->list[shapeTree->count] = *shape;
    shapeTree->count++;
}

Shape* getShapeFromID(VM* vm, int id) {
    return &vm->shapes.list[id];
}

IndexMap* getShapeIndexes(VM* vm, int id) {
    return &vm->shapes.list[id].indexes;
}

int createShapeFromParent(VM* vm, int parentID, ObjString* edge) {
    ShapeTree* shapeTree = &vm->shapes;
    if (shapeTree->count >= INT32_MAX) {
        runtimeError(vm, "Too many shapes have been created.");
        return -1;
    }
    Shape* parentShape = &shapeTree->list[parentID];

    Shape newShape = {
        .id = shapeTree->count,
        .parentID = parentID,
        .type = parentShape->nextIndex <= UINT4_MAX ? SHAPE_NORMAL : SHAPE_COMPLEX,
        .nextIndex = parentShape->nextIndex + 1
    };
    initIndexMap(&newShape.edges);
    initIndexMap(&newShape.indexes);

    indexMapAddAll(vm, &parentShape->indexes, &newShape.indexes);
    indexMapSet(vm, &newShape.indexes, edge, parentShape->nextIndex);
    indexMapSet(vm, &parentShape->edges, edge, newShape.id);
    appendToShapeTree(vm, &newShape);
    return newShape.id;
}

int transitionShapeForObject(VM* vm, ObjInstance* object, ObjString* edge) {
    int parentID = object->shapeID;
    Shape* parentShape = &vm->shapes.list[parentID];
    int index;

    if (indexMapGet(&parentShape->edges, edge, &index)) object->shapeID = index;
    else object->shapeID = createShapeFromParent(vm, parentID, edge);
    return object->shapeID;
}

int getShapeFromParent(VM* vm, int parentID, ObjString* edge) {
    ShapeTree* shapeTree = &vm->shapes;
    if (shapeTree->count >= UINT32_MAX) {
        runtimeError(vm, "Too many shapes have been created.");
        return -1;
    }
    Shape* parentShape = &shapeTree->list[parentID];

    int index;
    if (indexMapGet(&parentShape->edges, edge, &index)) return index;
    else return createShapeFromParent(vm, parentID, edge);
}

int getIndexFromObjectShape(VM* vm, ObjInstance* object, ObjString* edge) {
    IndexMap* indexMap = &vm->shapes.list[object->shapeID].indexes;
    int index;
    if (indexMapGet(indexMap, edge, &index)) return index;
    return -1;
}