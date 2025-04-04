#include <stdlib.h>

#include "compiler_v1.h"
#include "memory.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

void* reallocate(VM* vm, void* pointer, size_t oldSize, size_t newSize) {
    vm->bytesAllocated += newSize - oldSize;
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage(vm);
#endif

        if (vm->bytesAllocated > vm->nextGC) {
            collectGarbage(vm);
        }
    }

    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

static void initGCRememberedSet(GCRememberedSet* rememberedSet) {
    rememberedSet->capacity = 0;
    rememberedSet->count = 0;
    rememberedSet->entries = NULL;
}

static void freeGCRememeberedSet(VM* vm, GCRememberedSet* rememberedSet) {
    FREE_ARRAY(GCRememberedEntry, rememberedSet->entries, rememberedSet->capacity);
    initGCRememberedSet(rememberedSet);
}

static void initGCGenerations(GC* gc, size_t heapSizes[]) {
    for (int i = 0; i <= GC_GENERATION_TYPE_PERMANENT; i++) {
        gc->generations[i] = (GCGeneration*)malloc(sizeof(GCGeneration));
        if (gc->generations[i] != NULL) {
            gc->generations[i]->bytesAllocated = 0;
            gc->generations[i]->heapSize = heapSizes[i]; // will update later.
            gc->generations[i]->objects = NULL;
            gc->generations[i]->type = i;
            initGCRememberedSet(&gc->generations[i]->rememberdSet);
        }
        else {
            fprintf(stderr, "Not enough memory to allocate heaps for garbage collector.");
            exit(74);
        }
    }
}

static void freeGCGenerations(VM* vm) {
    for (int i = 0; i <= GC_GENERATION_TYPE_PERMANENT; i++) {
        freeGCRememeberedSet(vm, &vm->gc->generations[i]->rememberdSet);
        free(vm->gc->generations[i]);
    }
}

GC* newGC(VM* vm) {
    GC* gc = (GC*)malloc(sizeof(GC));
    if (gc != NULL) {
        size_t heapSizes[] = { vm->config.gcEdenHeapSize, vm->config.gcYoungHeapSize, vm->config.gcOldHeapSize, vm->config.gcHeapSize };
        initGCGenerations(gc, heapSizes);
        gc->grayCapacity = 0;
        gc->grayCount = 0;
        gc->grayStack = NULL;
        return gc;
    }

    fprintf(stderr, "Not enough memory to allocate for garbage collector.");
    exit(74);
}

void freeGC(VM* vm) {
    freeGCGenerations(vm);
    free(vm->gc);
}

static GCRememberedEntry* findRememberedSetEntry(GCRememberedEntry* entries, int capacity, Obj* object) {
    uint64_t hash = (object->type == OBJ_INSTANCE) ? (object->objectID >> 2) : (object->objectID >> 2) + 1;
    uint32_t index = (uint32_t)hash & ((uint32_t)capacity - 1);
    for (;;) {
        GCRememberedEntry* entry = &entries[index];
        if (entry->object == NULL || entry->object == object) {
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

bool rememberedSetGetObject(GCRememberedSet* rememberedSet, Obj* object) {
    if (rememberedSet->count == 0) return false;
    GCRememberedEntry* entry = findRememberedSetEntry(rememberedSet->entries, rememberedSet->capacity, object);
    if (entry->object == NULL) return false;
    return true;
}

static void rememberedSetAdjustCapacity(VM* vm, GCRememberedSet* rememberedSet, int capacity) {
    GCRememberedEntry* entries = ALLOCATE(GCRememberedEntry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].object = NULL;
    }

    rememberedSet->count = 0;
    for (int i = 0; i < rememberedSet->capacity; i++) {
        GCRememberedEntry* entry = &rememberedSet->entries[i];
        if (entry->object == NULL) continue;

        GCRememberedEntry* dest = findRememberedSetEntry(entries, capacity, entry->object);
        dest->object = entry->object;
        rememberedSet->count++;
    }

    FREE_ARRAY(GCRememberedEntry, rememberedSet->entries, rememberedSet->capacity);
    rememberedSet->entries = entries;
    rememberedSet->capacity = capacity;
}

bool rememberedSetPutObject(VM* vm, GCRememberedSet* rememberedSet, Obj* object) {
    ENSURE_OBJECT_ID(object);
    if (rememberedSet->count + 1 > rememberedSet->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(rememberedSet->capacity);
        rememberedSetAdjustCapacity(vm, rememberedSet, capacity);
    }

    GCRememberedEntry* entry = findRememberedSetEntry(rememberedSet->entries, rememberedSet->capacity, object);
    bool isNewObject = entry->object == NULL;
    if (isNewObject) {
#ifdef DEBUG_LOG_GC
        printf("%p added to remembered set ", (void*)object);
        printValue(OBJ_VAL(object));
        printf("\n");
#endif
        rememberedSet->count++;
    }

    entry->object = object;
    return isNewObject;
}

void markRememberedSet(VM* vm, GCGenerationType generation) {
    GCRememberedSet* rememberedSet = &vm->gc->generations[generation]->rememberdSet;
    for (int i = 0; i < rememberedSet->capacity; i++) {
        GCRememberedEntry* entry = &rememberedSet->entries[i];
        markObject(vm, entry->object);
    }
}

void markObject(VM* vm, Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    object->isMarked = true;
    if (vm->gc->grayCapacity < vm->gc->grayCount + 1) {
        vm->gc->grayCapacity = GROW_CAPACITY(vm->gc->grayCapacity);
        Obj** grayStack = (Obj**)realloc(vm->gc->grayStack, sizeof(Obj*) * vm->gc->grayCapacity);
        if (grayStack == NULL) exit(1);
        vm->gc->grayStack = grayStack;
    }
    vm->gc->grayStack[vm->gc->grayCount++] = object;
}

void markValue(VM* vm, Value value) {
    if (IS_OBJ(value)) markObject(vm, AS_OBJ(value));
}

static void markArray(VM* vm, ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(vm, array->values[i]);
    }
}

static void blackenObject(VM* vm, Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            markArray(vm, &array->elements);
            break;
        }
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* boundMethod = (ObjBoundMethod*)object;
            markValue(vm, boundMethod->receiver);
            markValue(vm, boundMethod->method);
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            markObject(vm, (Obj*)klass->name);
            markObject(vm, (Obj*)klass->fullName);
            markObject(vm, (Obj*)klass->superclass);
            markObject(vm, (Obj*)klass->obj.klass);
            markObject(vm, (Obj*)klass->namespace);
            markArray(vm, &klass->traits);
            markIDMap(vm, &klass->indexes);
            markArray(vm, &klass->fields);
            markTable(vm, &klass->methods);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            markObject(vm, (Obj*)closure->function);
            markObject(vm, (Obj*)closure->module);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject(vm, (Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_DICTIONARY: {
            ObjDictionary* dict = (ObjDictionary*)object;
            for (int i = 0; i < dict->capacity; i++) {
                ObjEntry* entry = &dict->entries[i];
                markObject(vm, (Obj*)entry);
            }
            break;
        }
        case OBJ_ENTRY: {
            ObjEntry* entry = (ObjEntry*)object;
            markValue(vm, entry->key);
            markValue(vm, entry->value);
            break;
        }
        case OBJ_EXCEPTION: { 
            ObjException* exception = (ObjException*)object;
            markObject(vm, (Obj*)exception->message);
            markObject(vm, (Obj*)exception->stacktrace);
            break;
        }
        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)object;
            markObject(vm, (Obj*)file->name);
            markObject(vm, (Obj*)file->mode);
            break;
        }
        case OBJ_FRAME: {
            ObjFrame* frame = (ObjFrame*)object;
            markObject(vm, (Obj*)frame->closure);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            markObject(vm, (Obj*)function->name);
            markArray(vm, &function->chunk.constants);
            markArray(vm, &function->chunk.identifiers);
            break;
        }
        case OBJ_GENERATOR: {
            ObjGenerator* generator = (ObjGenerator*)object;
            markObject(vm, (Obj*)generator->frame);
            markObject(vm, (Obj*)generator->outer);
            markObject(vm, (Obj*)generator->inner);
            markValue(vm, generator->value);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            markObject(vm, (Obj*)object->klass);
            markArray(vm, &instance->fields);
            break;
        }
        case OBJ_METHOD: {
            ObjMethod* method = (ObjMethod*)object;
            markObject(vm, (Obj*)method->behavior);
            markObject(vm, (Obj*)method->closure);
            break;
        }
        case OBJ_MODULE: {
            ObjModule* module = (ObjModule*)object;
            markObject(vm, (Obj*)module->path);
            if (module->closure != NULL) markObject(vm, (Obj*)module->closure);
            markIDMap(vm, &module->valIndexes);
            markArray(vm, &module->valFields);
            markIDMap(vm, &module->varIndexes);
            markArray(vm, &module->varFields);
            break;
        }
        case OBJ_NAMESPACE: {
            ObjNamespace* namespace = (ObjNamespace*)object;
            markObject(vm, (Obj*)namespace->shortName);
            markObject(vm, (Obj*)namespace->fullName);
            markObject(vm, (Obj*)namespace->enclosing);
            markTable(vm, &namespace->values);
            break;
        }
        case OBJ_NATIVE_FUNCTION: {
            ObjNativeFunction* nativeFunction = (ObjNativeFunction*)object;
            markObject(vm, (Obj*)nativeFunction->name);
            break;
        }
        case OBJ_NATIVE_METHOD: {
            ObjNativeMethod* nativeMethod = (ObjNativeMethod*)object;
            markObject(vm, (Obj*)nativeMethod->klass);
            markObject(vm, (Obj*)nativeMethod->name);
            break;
        }
        case OBJ_NODE: {
            ObjNode* node = (ObjNode*)object;
            markValue(vm, node->element);
            markObject(vm, (Obj*)node->prev);
            markObject(vm, (Obj*)node->next);
            break;
        }
        case OBJ_PROMISE: {
            ObjPromise* promise = (ObjPromise*)object;
            markValue(vm, promise->value);
            markObject(vm, (Obj*)promise->captures);
            markObject(vm, (Obj*)promise->exception);
            markValue(vm, promise->executor);
            markArray(vm, &promise->handlers);
            break;
        }
        case OBJ_RECORD: {
            ObjRecord* record = (ObjRecord*)object;
            if (record->markFunction) record->markFunction(record->data);
            break;
        }
        case OBJ_TIMER: { 
            ObjTimer* timer = (ObjTimer*)object;
            if (timer->timer != NULL && timer->timer->data != NULL) {
                TimerData* data = (TimerData*)timer->timer->data;
                markValue(vm, data->receiver);
                markObject(vm, (Obj*)data->closure);
            }
            break;
        }
        case OBJ_UPVALUE:
            markValue(vm, ((ObjUpvalue*)object)->closed);
            break;
        case OBJ_VALUE_INSTANCE: { 
            ObjValueInstance* instance = (ObjValueInstance*)object;
            markObject(vm, (Obj*)object->klass);
            markArray(vm, &instance->fields);
            break;
        }
        default:
            break;
    }
}

static void freeObject(VM* vm, Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

    switch (object->type) {
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            freeValueArray(vm, &array->elements);
            FREE(ObjArray, object);
            break;
        }
        case OBJ_BOUND_METHOD: 
            FREE(ObjBoundMethod, object);
            break;       
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            freeValueArray(vm, &klass->traits);
            freeIDMap(vm, &klass->indexes);
            freeValueArray(vm, &klass->fields);
            freeTable(vm, &klass->methods);
            FREE(ObjClass, object);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_DICTIONARY: {
            ObjDictionary* dict = (ObjDictionary*)object;
            FREE_ARRAY(ObjEntry, dict->entries, dict->capacity);
            FREE(ObjDictionary, object);
            break;
        }
        case OBJ_ENTRY: {
            FREE(ObjEntry, object);
            break;
        }
        case OBJ_EXCEPTION: { 
            FREE(ObjException, object);
            break;
        }
        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)object;
            if (file->fsStat != NULL) {
                uv_fs_req_cleanup(file->fsStat);
                free(file->fsStat);
            }
            if (file->fsOpen != NULL) {
                uv_fs_req_cleanup(file->fsOpen);
                free(file->fsOpen);
            }
            if (file->fsRead != NULL) {
                uv_fs_req_cleanup(file->fsRead);
                free(file->fsRead);
            }
            if (file->fsWrite != NULL) {
                uv_fs_req_cleanup(file->fsWrite);
                free(file->fsWrite);
            }
            FREE(ObjFile, object);
            break;
        }
        case OBJ_FRAME: {
            FREE(ObjFrame, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(vm, &function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_GENERATOR: {
            FREE(ObjGenerator, object);
            break; 
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            freeValueArray(vm, &instance->fields);
            FREE(ObjInstance, object);
            break;
        }
        case OBJ_METHOD: {
            FREE(ObjMethod, object);
            break;
        }
        case OBJ_MODULE: {
            ObjModule* module = (ObjModule*)object;
            freeIDMap(vm, &module->valIndexes);
            freeValueArray(vm, &module->valFields);
            freeIDMap(vm, &module->varIndexes);
            freeValueArray(vm, &module->varFields);
            FREE(ObjModule, object);
            break;
        }             
        case OBJ_NAMESPACE: { 
            ObjNamespace* namespace = (ObjNamespace*)object;
            freeTable(vm, &namespace->values);
            FREE(ObjNamespace, object);
            break;
        }
        case OBJ_NATIVE_FUNCTION:
            FREE(ObjNativeFunction, object);
            break;
        case OBJ_NATIVE_METHOD:
            FREE(ObjNativeMethod, object);
            break;
        case OBJ_NODE: {
            FREE(ObjNode, object);
            break;
        }
        case OBJ_PROMISE: {
            ObjPromise* promise = (ObjPromise*)object;
            freeValueArray(vm, &promise->handlers);
            FREE(ObjPromise, object);
            break;
        }
        case OBJ_RANGE: {
            FREE(ObjRange, object);
            break;
        }
        case OBJ_RECORD: {
            ObjRecord* record = (ObjRecord*)object;
            if (record->freeFunction) record->freeFunction(record->data);
            else if(record->shouldFree) free(record->data);
            FREE(ObjRecord, object);
            break;
        }
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            reallocate(vm, object, sizeof(ObjString) + string->length + 1, 0);
            break;
        } 
        case OBJ_TIMER: { 
            ObjTimer* timer = (ObjTimer*)object;
            FREE(ObjTimer, object);
            break;
        }
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
        case OBJ_VALUE_INSTANCE: { 
            ObjValueInstance* instance = (ObjValueInstance*)object;
            freeValueArray(vm, &instance->fields);
            FREE(ObjValueInstance, object);
            break;
        }
    }
}

static void markRoots(VM* vm) {
    for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
        markValue(vm, *slot);
    }

    for (int i = 0; i < vm->frameCount; i++) {
        markObject(vm, (Obj*)vm->frames[i].closure);
    }

    for (ObjUpvalue* upvalue = vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject(vm, (Obj*)upvalue);
    }

    markTable(vm, &vm->classes);
    markTable(vm, &vm->namespaces);
    markTable(vm, &vm->modules);
    markRememberedSet(vm, GC_GENERATION_TYPE_EDEN);
    markCompilerRoots(vm);
    markObject(vm, (Obj*)vm->initString);
    markObject(vm, (Obj*)vm->runningGenerator);
}

static void traceReferences(VM* vm) {
    while (vm->gc->grayCount > 0) {
        Obj* object = vm->gc->grayStack[--vm->gc->grayCount];
        blackenObject(vm, object);
    }
}

static void sweep(VM* vm) {
    Obj* previous = NULL;
    Obj* object = vm->gc->generations[GC_GENERATION_TYPE_EDEN]->objects;
    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } 
        else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } 
            else {
                vm->gc->generations[GC_GENERATION_TYPE_EDEN]->objects = object;
            }
            freeObject(vm, unreached);
        }
    }
}

void collectGarbage(VM* vm) {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm->bytesAllocated;
#endif

    markRoots(vm);
    traceReferences(vm);
    tableRemoveWhite(&vm->strings);
    sweep(vm);
    vm->nextGC = vm->bytesAllocated * vm->config.gcGrowthFactor;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n", before - vm->bytesAllocated, before, vm->bytesAllocated, vm->nextGC);
#endif
}

void freeObjects(VM* vm) {
    Obj* object = vm->gc->generations[GC_GENERATION_TYPE_EDEN]->objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(vm, object);
        object = next;
    }
    free(vm->gc->grayStack);
}