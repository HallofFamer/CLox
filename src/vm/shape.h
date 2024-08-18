#pragma once
#ifndef clox_shape_h
#define clox_shape_h

#include "id.h"
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
    IDMap edges;
    IDMap indexes;
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
IDMap* getShapeIndexes(VM* vm, int id);
int getDefaultShapeIDForObject(Obj* object);
int createShapeFromParent(VM* vm, int parentID, ObjString* edge);
int transitionShapeForObject(VM* vm, Obj* object, ObjString* edge);
int getIndexFromObjectShape(VM* vm, Obj* object, ObjString* edge);

#endif // !clox_shape_h
