#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "shape.h"

static void initRootShape(Shape* shape) {
    shape->id = 0;
    shape->parentID = -1;
    shape->type = SHAPE_ROOT;
    shape->nextIndex = 0;
    initIDMap(&shape->edges);
    initIDMap(&shape->indexes);

#ifdef DEBUG_PRINT_SHAPE
    printf("Shape ID: %d, Parent ID: %d, shape type: %d, next index: %d\n\n", shape->id, shape->parentID, shape->type, shape->nextIndex);
#endif
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

IDMap* getShapeIndexes(VM* vm, int id) {
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
    initIDMap(&newShape.edges);
    initIDMap(&newShape.indexes);

    idMapAddAll(vm, &parentShape->indexes, &newShape.indexes);
    idMapSet(vm, &newShape.indexes, edge, parentShape->nextIndex);
    idMapSet(vm, &parentShape->edges, edge, newShape.id);
    appendToShapeTree(vm, &newShape);

#ifdef DEBUG_PRINT_SHAPE
    printf("Shape ID: %d, Parent ID: %d, shape type: %d, next index: %d\n", newShape.id, newShape.parentID, newShape.type, newShape.nextIndex);
    for (int i = 0; i < newShape.indexes.capacity; i++) {
        IndexEntry* entry = &newShape.indexes.entries[i];
        if (entry->key != NULL) {
            printf("Property at index %d: '%s'\n", entry->value, entry->key->chars);
        }
    }
    printf("\n");
#endif

    return newShape.id;
}

int transitionShapeForObject(VM* vm, Obj* object, ObjString* edge) {
    int parentID = object->shapeID;
    Shape* parentShape = &vm->shapes.list[parentID];
    int index;

    if (idMapGet(&parentShape->edges, edge, &index)) object->shapeID = index;
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
    if (idMapGet(&parentShape->edges, edge, &index)) return index;
    else return createShapeFromParent(vm, parentID, edge);
}

int getIndexFromObjectShape(VM* vm, Obj* object, ObjString* edge) {
    IDMap* idMap = &vm->shapes.list[object->shapeID].indexes;
    int index;
    if (idMapGet(idMap, edge, &index)) return index;
    return -1;
}