#pragma once
#ifndef clox_shape_h
#define clox_shape_h

#include "common.h"
#include "index.h"
#include "object.h"

typedef enum {
    SHAPE_ROOT,
    SHAPE_NORMAL,
    SHAPE_COMPLEX,
    SHAPE_INVALID
} ShapeType;

typedef struct {
    int id;
    int parentID;
    ShapeType type;
    IndexMap edges;
    IndexMap indexes;
    int nextIndex;
} Shape;

typedef struct {
    Shape* list;
    int count;
    int capacity;
    Shape* rootShape;
} ShapeTree;

static inline Shape* getShapeFromID(VM* vm, int id) {
    return &vm->shapes.list[id];
}

static inline IndexMap* getShapeIndexes(VM* vm, int id) {
    return &vm->shapes.list[id].indexes;
}

void initShapeTree(VM* vm);
void freeShapeTree(VM* vm, ShapeTree* shapeTree);
void appendToShapeTree(VM* vm, Shape* shape);
int createShapeFromParent(VM* vm, int parentID, ObjString* edge);
int transitionShapeForObject(VM* vm, ObjInstance* object, ObjString* edge);
int getIndexFromObjectShape(VM* vm, ObjInstance* object, ObjString* edge);

#endif // !clox_shape_h
