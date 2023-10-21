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

void initShapeTree(VM* vm);
void freeShapeTree(VM* vm, ShapeTree* shapeTree);
void appendToShapeTree(VM* vm, Shape* shape);
Shape* getShapeFromID(VM* vm, int id);
IndexMap* getShapeIndexes(VM* vm, int id);
int createShapeFromParent(VM* vm, int parentID, ObjString* edge);
int transitionShapeForObject(VM* vm, ObjInstance* object, ObjString* edge);
int getIndexFromObjectShape(VM* vm, ObjInstance* object, ObjString* edge);

#endif // !clox_shape_h
