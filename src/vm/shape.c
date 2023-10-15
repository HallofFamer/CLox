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
    initTable(&newShape.edges);
    initTable(&newShape.indexes);

    tableAddAll(vm, &parentShape->indexes, &newShape.indexes);
    tableSet(vm, &newShape.indexes, edge, INT_VAL(parentShape->nextIndex));
    tableSet(vm, &parentShape->edges, edge, INT_VAL(newShape.id));
    appendToShapeTree(vm, &newShape);
    return newShape.id;
}

int transitionShapeForObject(VM* vm, ObjInstance* object, ObjString* edge) {
    int parentID = object->shapeID;
    Shape* parentShape = &vm->shapes.list[parentID];
    Value value;

    if (tableGet(&parentShape->edges, edge, &value)) object->shapeID = AS_INT(value);
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

    Value value;
    if (tableGet(&parentShape->edges, edge, &value)) return AS_INT(value);
    else return createShapeFromParent(vm, parentID, edge);
}