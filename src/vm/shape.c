#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "shape.h"

int defaultShapeIDs[OBJ_VOID];

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

static void createDefaultShapes(VM* vm) {
    int shapeIDLength = createShapeFromParent(vm, 0, newString(vm, "length"));
    defaultShapeIDs[OBJ_ARRAY] = shapeIDLength;

    int shapeIDReceiver = createShapeFromParent(vm, 0, newString(vm, "receiver"));
    int shapeIDBoundMethod = createShapeFromParent(vm, shapeIDReceiver, newString(vm, "method"));
    defaultShapeIDs[OBJ_BOUND_METHOD] = shapeIDBoundMethod;

    int shapeIDName = createShapeFromParent(vm, 0, newString(vm, "name"));
    int shapeIDClass1 = createShapeFromParent(vm, shapeIDName, newString(vm, "namespace"));
    int shapeIDClass2 = createShapeFromParent(vm, shapeIDClass1, newString(vm, "superclass"));
    defaultShapeIDs[OBJ_CLASS] = shapeIDClass2;

    int shapeIDClosure = createShapeFromParent(vm, shapeIDName, newString(vm, "arity"));
    defaultShapeIDs[OBJ_CLOSURE] = shapeIDClosure;

    int shapeIDDictionary = createShapeFromParent(vm, shapeIDLength, newString(vm, "entries"));
    defaultShapeIDs[OBJ_DICTIONARY] = shapeIDDictionary;

    int shapeIDKey = createShapeFromParent(vm, 0, newString(vm, "key"));
    int shapeIDEntry = createShapeFromParent(vm, shapeIDKey, newString(vm, "value"));
    defaultShapeIDs[OBJ_ENTRY] = shapeIDEntry;

    int shapeIDMessage = createShapeFromParent(vm, 0, newString(vm, "message"));
    int shapeIDException = createShapeFromParent(vm, shapeIDMessage, newString(vm, "stacktrace"));
    defaultShapeIDs[OBJ_EXCEPTION] = shapeIDException;

    int shapeIDFile = createShapeFromParent(vm, shapeIDName, newString(vm, "mode"));
    defaultShapeIDs[OBJ_FILE] = shapeIDFile;

    defaultShapeIDs[OBJ_FUNCTION] = -1;
    defaultShapeIDs[OBJ_GENERIC] = -1;
    defaultShapeIDs[OBJ_INSTANCE] = 0;

    int shapeIDMethod = createShapeFromParent(vm, shapeIDClosure, newString(vm, "behavior"));
    defaultShapeIDs[OBJ_METHOD] = shapeIDMethod;
    defaultShapeIDs[OBJ_MODULE] = -1;
    
    int shapeIDShortName = createShapeFromParent(vm, 0, newString(vm, "shortName"));
    int shapeIDNamespace1 = createShapeFromParent(vm, shapeIDShortName, newString(vm, "fullName"));
    int shapeIDNamespace2 = createShapeFromParent(vm, shapeIDNamespace1, newString(vm, "enclosing"));
    defaultShapeIDs[OBJ_NAMESPACE] = shapeIDNamespace2;

    defaultShapeIDs[OBJ_NATIVE_FUNCTION] = -1;
    defaultShapeIDs[OBJ_NATIVE_METHOD] = -1;

    int shapeIDElement = createShapeFromParent(vm, 0, newString(vm, "element"));
    int shapeIDNode1 = createShapeFromParent(vm, shapeIDElement, newString(vm, "prev"));
    int shapeIDNode2 = createShapeFromParent(vm, shapeIDNode1, newString(vm, "next"));
    defaultShapeIDs[OBJ_NODE] = shapeIDNode2;

    int shapeIDFrom = createShapeFromParent(vm, 0, newString(vm, "from"));
    int shapeIDRange = createShapeFromParent(vm, shapeIDFrom, newString(vm, "to"));
    defaultShapeIDs[OBJ_RANGE] = shapeIDRange;

    defaultShapeIDs[OBJ_RECORD] = -1;
    defaultShapeIDs[OBJ_STRING] = shapeIDLength;
    defaultShapeIDs[OBJ_UPVALUE] = -1;
}

void initShapeTree(VM* vm) {
    ShapeTree shapeTree = { .list = NULL, .count = 0, .capacity = 0, .rootShape = NULL };
    vm->shapes = shapeTree;

    Shape rootShape;
    initRootShape(&rootShape);
    appendToShapeTree(vm, &rootShape);
    createDefaultShapes(vm);
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

int getDefaultShapeIDForObject(Obj* object) {
    return defaultShapeIDs[object->type];
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