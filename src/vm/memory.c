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

void markObject(VM* vm, Obj* object) {
    if (object == NULL) return;
    if (object->isMarked) return;

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    object->isMarked = true;
    if (vm->grayCapacity < vm->grayCount + 1) {
        vm->grayCapacity = GROW_CAPACITY(vm->grayCapacity);
        Obj** grayStack = (Obj**)realloc(vm->grayStack, sizeof(Obj*) * vm->grayCapacity);
        if (grayStack == NULL) exit(1);
        vm->grayStack = grayStack;
    }
    vm->grayStack[vm->grayCount++] = object;
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
    markCompilerRoots(vm);
    markObject(vm, (Obj*)vm->initString);
    markObject(vm, (Obj*)vm->runningGenerator);
}

static void traceReferences(VM* vm) {
    while (vm->grayCount > 0) {
        Obj* object = vm->grayStack[--vm->grayCount];
        blackenObject(vm, object);
    }
}

static void sweep(VM* vm) {
    Obj* previous = NULL;
    Obj* object = vm->objects;
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
                vm->objects = object;
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
    Obj* object = vm->objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(vm, object);
        object = next;
    }
    free(vm->grayStack);
}