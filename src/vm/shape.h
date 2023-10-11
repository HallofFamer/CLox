#pragma once
#ifndef clox_shape_h
#define clox_shape_h

#include "common.h"
#include "object.h"
#include "table.h"

typedef enum {
    SHAPE_ROOT,
    SHAPE_NORMAL,
    SHAPE_COMPLEX,
    SHAPE_INVALID
} ShapeType;

typedef struct {
    uint32_t id;
    uint32_t parentID;
    ShapeType type;
    Table edges;
    Table indexes;
    uint32_t nextIndex;
} Shape;

typedef struct {
    Shape* list;
    uint32_t count;
    uint32_t capacity;
    Shape* rootShape;
} ShapeTree;

void initShapeTree(VM* vm);
void freeShapeTree(VM* vm, ShapeTree* shapeTree);
void shapeTreeAppend(VM* vm, Shape shape);

#endif // !clox_shape_h
