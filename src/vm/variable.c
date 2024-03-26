#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "namespace.h"
#include "variable.h"
#include "vm.h"

bool matchVariableName(ObjString* sourceString, const char* targetChars, int targetLength) {
    if (sourceString == NULL || sourceString->length != targetLength) return false;
    return memcmp(sourceString->chars, targetChars, targetLength) == 0;
}

static bool loadGlobalValue(VM* vm, Chunk* chunk, uint8_t byte, Value* value) {
    InlineCache* inlineCache = &chunk->inlineCaches[byte];
    ObjString* name = AS_STRING(chunk->identifiers.values[byte]);
    int index;
    if (idMapGet(&vm->currentModule->valIndexes, name, &index)) {
#ifdef DEBUG_TRACE_CACHE
        printf("Cache miss for getting immutable global variable: '%s' at index %d.\n", name->chars, inlineCache->index);
#endif 
        *value = vm->currentModule->valFields.values[index];
        writeInlineCache(inlineCache, CACHE_GVAL, (int)byte, index);
        return true;
    }
    return false;
}

static bool loadGlobalVariable(VM* vm, Chunk* chunk, uint8_t byte, Value* value) {
    InlineCache* inlineCache = &chunk->inlineCaches[byte];
    ObjString* name = AS_STRING(chunk->identifiers.values[byte]);
    int index;
    if (idMapGet(&vm->currentModule->varIndexes, name, &index)) {
#ifdef DEBUG_TRACE_CACHE
        printf("Cache miss for getting mutable global variable: '%s' at index %d.\n", name->chars, inlineCache->index);
#endif 
        *value = vm->currentModule->varFields.values[index];
        writeInlineCache(inlineCache, CACHE_GVAR, (int)byte, index);
        return true;
    }
    return false;
}

static bool loadGlobalFromTable(VM* vm, Chunk* chunk, uint8_t byte, Value* value) {
    InlineCache* inlineCache = &chunk->inlineCaches[byte];
    ObjString* name = AS_STRING(chunk->identifiers.values[byte]);
    if (loadGlobalValue(vm, chunk, byte, value)) return true;
    else if (loadGlobalVariable(vm, chunk, byte, value)) return true;
    else if (tableGet(&vm->currentNamespace->values, name, value)) return true;
    else return tableGet(&vm->rootNamespace->values, name, value);
}

static bool loadGlobalFromCache(VM* vm, Chunk* chunk, uint8_t byte, Value* value) {
    InlineCache* inlineCache = &chunk->inlineCaches[byte];
    if (inlineCache->id == byte) {
        switch (inlineCache->type) {
            case CACHE_GVAL: {
#ifdef DEBUG_TRACE_CACHE
                printf("Cache hit for getting immutable global variable: '%s' at index %d.\n", AS_CSTRING(chunk->identifiers.values[byte]), inlineCache->index);
#endif 
                *value = vm->currentModule->valFields.values[inlineCache->index];
                return true;
            }
            case CACHE_GVAR: {
#ifdef DEBUG_TRACE_CACHE
                printf("Cache hit for getting mutable global variable: '%s' at index %d.\n", AS_CSTRING(chunk->identifiers.values[byte]), inlineCache->index);
#endif 
                *value = vm->currentModule->varFields.values[inlineCache->index];
                return true;
            }
            default: return loadGlobalFromTable(vm, chunk, byte, value);
        }
    }
    else return loadGlobalFromTable(vm, chunk, byte, value);
}

bool loadGlobal(VM* vm, Chunk* chunk, uint8_t byte, Value* value) {
    if (chunk->inlineCaches[byte].type != CACHE_NONE) return loadGlobalFromCache(vm, chunk, byte, value);
    return loadGlobalFromTable(vm, chunk, byte, value);
}

bool hasInstanceVariable(VM* vm, Obj* object, Chunk* chunk, uint8_t byte) {
    ObjString* name = AS_STRING(chunk->identifiers.values[byte]);
    IDMap* idMap = getShapeIndexes(vm, object->shapeID);
    int index;
    return idMapGet(idMap, name, &index);
}

static void getAndPushGenericInstanceVariableByIndex(VM* vm, Obj* object, int index) {
    ValueArray* slots = getSlotsFromGenericObject(vm, object);
    int offset = getOffsetForGenericObject(object);
    push(vm, slots->values[index - offset]);
}

static bool getGenericInstanceVariableByIndex(VM* vm, Obj* object, int index) {
    switch (object->type) {
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            if (index == 0) push(vm, INT_VAL(array->elements.count));
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            if (index == 0) push(vm, bound->receiver);
            else if (index == 1) push(vm, OBJ_VAL(bound->method));
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            if (index == 0) push(vm, OBJ_VAL(closure->function->name));
            else if (index == 1) push(vm, INT_VAL(closure->function->arity));
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_DICTIONARY: {
            ObjDictionary* dictionary = (ObjDictionary*)object;
            if (index == 0) push(vm, INT_VAL(dictionary->count));
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_ENTRY: {
            ObjEntry* entry = (ObjEntry*)object;
            if (index == 0) push(vm, entry->key);
            else if (index == 1) push(vm, entry->value);
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_EXCEPTION: {
            ObjException* exception = (ObjException*)object;
            if (index == 0) push(vm, OBJ_VAL(exception->message));
            else if (index == 1) push(vm, OBJ_VAL(exception->stacktrace));
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)object;
            if (index == 0) push(vm, OBJ_VAL(file->name));
            else if (index == 1) push(vm, OBJ_VAL(file->mode));
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_GENERATOR: { 
            ObjGenerator* generator = (ObjGenerator*)object;
            if (index == 0) push(vm, INT_VAL(generator->state));
            else if (index == 1) push(vm, generator->value);
            else if (index == 2) push(vm, generator->outer != NULL ? OBJ_VAL(generator->outer) : NIL_VAL);
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_METHOD: {
            ObjMethod* method = (ObjMethod*)object;
            if (index == 0) push(vm, OBJ_VAL(method->closure->function->name));
            else if (index == 1) push(vm, INT_VAL(method->closure->function->arity));
            else if (index == 2) push(vm, OBJ_VAL(method->behavior));
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_NODE: {
            ObjNode* node = (ObjNode*)object;
            if (index == 0) push(vm, node->element);
            else if (index == 1) push(vm, OBJ_VAL(node->prev));
            else if (index == 2) push(vm, OBJ_VAL(node->next));
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_PROMISE: {
            ObjPromise* promise = (ObjPromise*)object;
            if (index == 0) push(vm, INT_VAL(promise->state));
            else if (index == 1) push(vm, promise->value);
            else if (index == 2) push(vm, INT_VAL(promise->id));
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_RANGE: {
            ObjRange* range = (ObjRange*)object;
            if (index == 0) push(vm, INT_VAL(range->from));
            else if (index == 1) push(vm, INT_VAL(range->to));
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_STRING: { 
            ObjString* string = (ObjString*)object;
            if (index == 0) push(vm, INT_VAL(string->length));
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_TIMER: { 
            ObjTimer* timer = (ObjTimer*)object;
            if (index == 0) push(vm, INT_VAL(timer->id));
            else if (index == 1) push(vm, BOOL_VAL(timer->isRunning));
            else getAndPushGenericInstanceVariableByIndex(vm, object, index);
            return true;
        }
        case OBJ_VALUE_INSTANCE: { 
            ObjValueInstance* instance = (ObjValueInstance*)object;
            push(vm, instance->fields.values[index]);
            return true;
        }
        default:
            runtimeError(vm, "Undefined property at index %d on Object type %d.", index, object->type);
            return false;
    }
}

static bool getAndPushGenericInstanceVariableByName(VM* vm, Obj* object, ObjString* name) {
    ENSURE_OBJECT_ID(object);
    ValueArray* slots = getSlotsFromGenericObject(vm, object);
    if (slots == NULL) {
        runtimeError(vm, "Generic object has no ID assigned.");
        return false;
    }

    int index = getIndexFromObjectShape(vm, object, name);
    if (index == -1) {
        runtimeError(vm, "Undefined property %s on Object %s", name->chars, object->klass->fullName->chars);
        return false;
    }

    int offset = getOffsetForGenericObject(object);
    push(vm, slots->values[index - offset]);
    return true;
}

static bool getGenericInstanceVariableByName(VM* vm, Obj* object, ObjString* name) {
    switch (object->type) {
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            if (matchVariableName(name, "length", 6)) push(vm, INT_VAL(array->elements.count));
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_BOUND_METHOD: {
           ObjBoundMethod* bound = (ObjBoundMethod*)object;
            if (matchVariableName(name, "receiver", 8) == 0) push(vm, OBJ_VAL(bound->receiver));
            else if (matchVariableName(name, "method", 6)) push(vm, OBJ_VAL(bound->method));
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            if (matchVariableName(name, "name", 4)) push(vm, OBJ_VAL(closure->function->name));
            else if (matchVariableName(name, "arity", 5)) push(vm, INT_VAL(closure->function->arity));
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_DICTIONARY: {
            ObjDictionary* dictionary = (ObjDictionary*)object;
            if (matchVariableName(name, "length", 6)) push(vm, INT_VAL(dictionary->count));
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_ENTRY: {
            ObjEntry* entry = (ObjEntry*)object;
            if (matchVariableName(name, "key", 3)) push(vm, entry->key);
            else if (matchVariableName(name, "value", 5)) push(vm, entry->value);
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_EXCEPTION: {
            ObjException* exception = (ObjException*)object;
            if (matchVariableName(name, "message", 7)) push(vm, OBJ_VAL(exception->message));
            else if (matchVariableName(name, "stacktrace", 10)) push(vm, OBJ_VAL(exception->stacktrace));
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)object;
            if (matchVariableName(name, "name", 4)) push(vm, OBJ_VAL(file->name));
            else if (matchVariableName(name, "mode", 4)) push(vm, OBJ_VAL(file->mode));
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_GENERATOR: {
            ObjGenerator* generator = (ObjGenerator*)object;
            if (matchVariableName(name, "state", 5)) push(vm, INT_VAL(generator->state));
            else if (matchVariableName(name, "value", 5)) push(vm, generator->value);
            else if (matchVariableName(name, "outer", 5)) push(vm, generator->outer != NULL ? OBJ_VAL(generator->outer) : NIL_VAL);
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_METHOD: {
            ObjMethod* method = (ObjMethod*)object;
            if (matchVariableName(name, "name", 4)) push(vm, OBJ_VAL(method->closure->function->name));
            else if (matchVariableName(name, "arity", 5)) push(vm, INT_VAL(method->closure->function->arity));
            else if (matchVariableName(name, "behavior", 8)) push(vm, OBJ_VAL(method->behavior));
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_NODE: {
            ObjNode* node = (ObjNode*)object;
            if (matchVariableName(name, "element", 7)) push(vm, node->element);
            else if (matchVariableName(name, "prev", 4)) push(vm, OBJ_VAL(node->prev));
            else if (matchVariableName(name, "next", 4)) push(vm, OBJ_VAL(node->next));
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_PROMISE: {
            ObjPromise* promise = (ObjPromise*)object;
            if (matchVariableName(name, "state", 5)) push(vm, INT_VAL(promise->state));
            else if (matchVariableName(name, "value", 5)) push(vm, promise->value);
            else if (matchVariableName(name, "id", 2)) push(vm, INT_VAL(promise->id));
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_RANGE: {
            ObjRange* range = (ObjRange*)object;
            if (matchVariableName(name, "from", 4)) push(vm, INT_VAL(range->from));
            else if (matchVariableName(name, "to", 2)) push(vm, INT_VAL(range->to));
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            if (matchVariableName(name, "length", 6)) push(vm, INT_VAL(string->length));
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_TIMER: { 
            ObjTimer* timer = (ObjTimer*)object;
            if (matchVariableName(name, "id", 2)) push(vm, INT_VAL(timer->id));
            else if (matchVariableName(name, "isRunning", 9)) push(vm, BOOL_VAL(timer->isRunning));
            else return getAndPushGenericInstanceVariableByName(vm, object, name);
            return true;
        }
        case OBJ_VALUE_INSTANCE: { 
            ObjValueInstance* instance = (ObjValueInstance*)object;
            int index = getIndexFromObjectShape(vm, object, name);
            if (index == -1) {
                runtimeError(vm, "Undefined property %s on instance.", name->chars);
                return false;
            }
            push(vm, instance->fields.values[index]);
            return true;
        }
        default:
            runtimeError(vm, "Undefined property %s on Object type %d.", name->chars, object->type);
            return false;
    }
}

static bool getGenericInstanceVariable(VM* vm, Obj* object, Chunk* chunk, uint8_t byte) {
    InlineCache* inlineCache = &chunk->inlineCaches[byte];
    int shapeID = object->shapeID;
    pop(vm);
    if (inlineCache->type == CACHE_IVAR && inlineCache->id == shapeID) {
#ifdef DEBUG_TRACE_CACHE
        printf("Cache hit for getting instance variable: '%s' from Shape ID %d at index %d.\n", AS_CSTRING(chunk->identifiers.values[byte]), inlineCache->id, inlineCache->index);
#endif  
        return getGenericInstanceVariableByIndex(vm, object, inlineCache->index);
    }

#ifdef DEBUG_TRACE_CACHE
    printf("Cache miss for getting instance variable: '%s' from Shape ID %d.\n", AS_CSTRING(chunk->identifiers.values[byte]), shapeID);
#endif

    ObjString* name = AS_STRING(chunk->identifiers.values[byte]);
    IDMap* idMap = getShapeIndexes(vm, shapeID);
    int index;
    if (idMapGet(idMap, name, &index)) {
        writeInlineCache(inlineCache, CACHE_IVAR, shapeID, index);
        return getGenericInstanceVariableByIndex(vm, object, index);
    }
    return getGenericInstanceVariableByName(vm, object, name);
}

bool getInstanceVariable(VM* vm, Value receiver, Chunk* chunk, uint8_t byte) {
    InlineCache* inlineCache = &chunk->inlineCaches[byte];
    if (IS_INSTANCE(receiver)) {
        ObjInstance* instance = AS_INSTANCE(receiver);
        int shapeID = instance->obj.shapeID;
        if (inlineCache->type == CACHE_IVAR && inlineCache->id == shapeID) {
#ifdef DEBUG_TRACE_CACHE
            printf("Cache hit for getting instance variable: '%s' from Shape ID %d at index %d.\n", AS_CSTRING(chunk->identifiers.values[byte]), inlineCache->id, inlineCache->index);
#endif 
            Value value = instance->fields.values[inlineCache->index];
            pop(vm);
            push(vm, value);
            return true;
        }

#ifdef DEBUG_TRACE_CACHE
        printf("Cache miss for getting instance variable: '%s' from Shape ID %d.\n", AS_CSTRING(chunk->identifiers.values[byte]), shapeID);
#endif

        ObjString* name = AS_STRING(chunk->identifiers.values[byte]);
        IDMap* idMap = getShapeIndexes(vm, shapeID);
        int index;

        if (idMapGet(idMap, name, &index)) {
            Value value = instance->fields.values[index];
            pop(vm);
            push(vm, value);
            writeInlineCache(inlineCache, CACHE_IVAR, shapeID, index);
            return true;
        }

        if (!bindMethod(vm, instance->obj.klass, name)) {
            return false;
        }
    }
    else if (IS_CLASS(receiver)) {
        ObjClass* klass = AS_CLASS(receiver);
        if (inlineCache->type == CACHE_CVAR && inlineCache->id == klass->behaviorID) {
#ifdef DEBUG_TRACE_CACHE
            printf("Cache hit for getting class variable: '%s' from Behavior ID %d at index %d.\n", AS_CSTRING(chunk->identifiers.values[byte]), inlineCache->id, inlineCache->index);
#endif 

            Value value = klass->fields.values[inlineCache->index];
            pop(vm);
            push(vm, value);
            return true;
        }

#ifdef DEBUG_TRACE_CACHE
        printf("Cache miss for getting class variable: '%s' from Behavior ID %d.\n", AS_CSTRING(chunk->identifiers.values[byte]), inlineCache->id);
#endif

        ObjString* name = AS_STRING(chunk->identifiers.values[byte]);
        int index;

        if (idMapGet(&klass->indexes, name, &index)) {
            Value value = klass->fields.values[index];
            pop(vm);
            push(vm, value);
            writeInlineCache(inlineCache, CACHE_CVAR, klass->behaviorID, index);
            return true;
        }
        
        runtimeError(vm, "Undefined property %s on class %s", name->chars, klass->fullName->chars);
        return false;
    }
    else if (IS_NAMESPACE(receiver)) {
        ObjNamespace* enclosing = AS_NAMESPACE(receiver);
        ObjString* name = AS_STRING(chunk->identifiers.values[byte]);
        Value value;

        if (tableGet(&enclosing->values, name, &value)) {
            pop(vm);
            push(vm, value);
            return true;
        }
        else {
            ObjString* filePath = resolveSourceFile(vm, name, enclosing);
            if (!sourceFileExists(filePath)) {
                runtimeError(vm, "Undefined class '%s.%s'.", enclosing->fullName->chars, name->chars);
                return false;
            }

            loadModule(vm, filePath);
            pop(vm);
            pop(vm);
            tableGet(&enclosing->values, name, &value);
            push(vm, value);
            return true;
        }
    }
    else if (IS_OBJ(receiver)) {
        return getGenericInstanceVariable(vm, AS_OBJ(receiver), chunk, byte);
    }
    else {
        if (IS_NIL(receiver)) runtimeError(vm, "Undefined property on nil.");
        return false;
    }

    return true;
}

static bool setAndPushGenericInstanceVariableByIndex(VM* vm, Obj* object, int index, Value value) {
    ValueArray* slots = getSlotsFromGenericObject(vm, object);
    int offset = getOffsetForGenericObject(object);
    slots->values[index - offset] = value;
    push(vm, value);
    return true;
}

static bool setGenericInstanceVariableByIndex(VM* vm, Obj* object, int index, Value value) {
    switch (object->type) {
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            if (index == 0) {
                runtimeError(vm, "Cannot set property length on Object Array.");
                return false;
            }
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);
        }
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            if (index == 0) bound->receiver = value;
            else if (index == 1 && IS_CLOSURE(value)) bound->method = AS_CLOSURE(value);
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);

            push(vm, value);
            return true;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            if (index <= 1) {
                runtimeError(vm, "Cannot set property name or arity on Object Function.");
                return false;
            }
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);
        }
        case OBJ_DICTIONARY: {
            ObjDictionary* dictionary = (ObjDictionary*)object;
            if (index == 0) {
                runtimeError(vm, "Cannot set property length on Object Dictionary.");
                return false;
            }
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);
        }
        case OBJ_ENTRY: {
            ObjEntry* entry = (ObjEntry*)object;
            if (index == 0) {
                runtimeError(vm, "Cannot set property key on Object Entry.");
                return false;
            }
            else if (index == 1) {
                entry->value = value;
                push(vm, value);
                return true;
            }
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);
        }
        case OBJ_EXCEPTION: {
            ObjException* exception = (ObjException*)object;
            if (index == 0 && IS_STRING(value)) exception->message = AS_STRING(value);
            else if (index == 1 && IS_ARRAY(value)) exception->stacktrace = AS_ARRAY(value);
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);

            push(vm, value);
            return true;
        }
        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)object;
            if (index == 0 && IS_STRING(value)) file->name = AS_STRING(value);
            else if (index == 1 && IS_STRING(value)) file->mode = AS_STRING(value);
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);

            push(vm, value);
            return true;
        }
        case OBJ_GENERATOR: {
            ObjGenerator* generator = (ObjGenerator*)object;
            if (index == 0 && IS_INT(value)) generator->state = AS_INT(value);
            else if (index == 1) generator->value = value;
            else if (index == 2 && IS_GENERATOR(value)) generator->outer = AS_GENERATOR(value);
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);

            push(vm, value);
            return true;
        }
        case OBJ_METHOD: {
            ObjMethod* method = (ObjMethod*)object;
            if (index <= 2) {
                runtimeError(vm, "Cannot set property name, arity or behavior on Object Method.");
                return false;
            }
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);
        }
        case OBJ_NODE: {
            ObjNode* node = (ObjNode*)object;
            if (index == 0) node->element = value;
            else if (index == 1 && IS_NODE(value)) node->prev = AS_NODE(value);
            else if (index == 2 && IS_NODE(value)) node->next = AS_NODE(value);
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);

            push(vm, value);
            return true;
        }
        case OBJ_PROMISE: {
            ObjPromise* promise = (ObjPromise*)object;
            if (index == 0 && IS_INT(value)) promise->state = AS_INT(value);
            else if (index == 1) promise->value = value;
            else if (index == 2) {
                runtimeError(vm, "Cannot set property id on Object Promise.");
                return false;
            }
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);

            push(vm, value);
            return true;
        }
        case OBJ_RANGE: {
            ObjRange* range = (ObjRange*)object;
            if (index == 0 && IS_INT(value)) range->from = AS_INT(value);
            else if (index == 1 && IS_INT(value)) range->to = AS_INT(value);
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);

            push(vm, value);
            return true;
        }
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            if (index == 0) {
                runtimeError(vm, "Cannot set property length on Object String.");
                return false;
            }
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);
        }
        case OBJ_TIMER: { 
            ObjTimer* timer = (ObjTimer*)object;
            if (index == 0 && IS_INT(value)) timer->id = AS_INT(value);
            else if (index == 1 && IS_BOOL(value)) timer->isRunning = AS_BOOL(value);
            else return setAndPushGenericInstanceVariableByIndex(vm, object, index, value);

            push(vm, value);
            return true;
        }
        default:
            runtimeError(vm, "Undefined property at index %d on Object type %d.", index, object->type);
            return false;
    }
}

static bool setAndPushGenericInstanceVariableByName(VM* vm, Obj* object, ObjString* name, Value value) {
    ENSURE_OBJECT_ID(object);
    ValueArray* slots = getSlotsFromGenericObject(vm, object);
    if (slots == NULL) {
        runtimeError(vm, "Generic object has no ID assigned.");
        return false;
    }

    int index = getIndexFromObjectShape(vm, object, name);
    if (index == -1) {
        transitionShapeForObject(vm, object, name);
        valueArrayWrite(vm, slots, value);
    }
    else slots->values[index] = value;

    push(vm, value);
    return true;
}

static bool setGenericInstanceVariableByName(VM* vm, Obj* object, ObjString* name, Value value) {
    switch (object->type) {
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            if (matchVariableName(name, "length", 6)) {
                runtimeError(vm, "Cannot set property length on Object Array.");
                return false;
            }
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);
        }
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            if (matchVariableName(name, "receiver", 8)) bound->receiver = value;
            else if (matchVariableName(name, "method", 6) && IS_CLOSURE(value)) bound->method = AS_CLOSURE(value);
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);

            push(vm, value);
            return true;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            if (matchVariableName(name, "name", 4) || matchVariableName(name, "arity", 5)) {
                runtimeError(vm, "Cannot set property %s on Object Function.", name->chars);
                return false;
            }
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);
        }
        case OBJ_DICTIONARY: {
            ObjDictionary* dictionary = (ObjDictionary*)object;
            if (matchVariableName(name, "length", 6)) {
                runtimeError(vm, "Cannot set property length on Object Dictionary.");
                return false;
            }
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);
        }
        case OBJ_ENTRY: {
            ObjEntry* entry = (ObjEntry*)object;
            if (matchVariableName(name, "key", 3)) {
                runtimeError(vm, "Cannot set property key on Object Entry.");
                return false;
            }
            else if (matchVariableName(name, "value", 5)) {
                entry->value = value;
                push(vm, value);
                return true;
            }
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);
        }
        case OBJ_EXCEPTION: {
            ObjException* exception = (ObjException*)object;
            if (matchVariableName(name, "message", 7) && IS_STRING(value)) exception->message = AS_STRING(value);
            else if (matchVariableName(name, "stacktrace", 10) && IS_ARRAY(value)) exception->stacktrace = AS_ARRAY(value);
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);

            push(vm, value);
            return true;
        }
        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)object;
            if (matchVariableName(name, "name", 4) && IS_STRING(value)) file->name = AS_STRING(value);
            else if (matchVariableName(name, "mode", 4) && IS_STRING(value)) file->mode = AS_STRING(value);
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);

            push(vm, value);
            return true;
        }
        case OBJ_GENERATOR: {
            ObjGenerator* generator = (ObjGenerator*)object;
            if (matchVariableName(name, "state", 5) && IS_INT(value)) generator->state = AS_INT(value);
            else if (matchVariableName(name, "value", 5)) generator->value = value;
            if (matchVariableName(name, "outer", 5) && IS_GENERATOR(value)) generator->outer = AS_GENERATOR(value);
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);

            push(vm, value);
            return true;
        }
        case OBJ_METHOD: {
            ObjMethod* method = (ObjMethod*)object;
            if (matchVariableName(name, "name", 4) || matchVariableName(name, "arity", 5) || matchVariableName(name, "behavior", 8)) {
                runtimeError(vm, "Cannot set property %s on Object Method.", name->chars);
                return false;
            }
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);
        }
        case OBJ_NODE: {
            ObjNode* node = (ObjNode*)object;
            if (matchVariableName(name, "element", 7)) node->element = value;
            else if (matchVariableName(name, "prev", 4) && IS_NODE(value)) node->prev = AS_NODE(value);
            else if (matchVariableName(name, "next", 4) && IS_NODE(value)) node->next = AS_NODE(value);
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);

            push(vm, value);
            return true;
        }
        case OBJ_PROMISE: {
            ObjPromise* promise = (ObjPromise*)object;
            if (matchVariableName(name, "state", 5) && IS_INT(value)) promise->state = AS_INT(value);
            else if (matchVariableName(name, "value", 5)) promise->value = value;
            else if (matchVariableName(name, "id", 2)) { 
                runtimeError(vm, "Cannot set property id on Object Promise.");
                return false;
            }
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);

            push(vm, value);
            return true;
        }
        case OBJ_RANGE: {
            ObjRange* range = (ObjRange*)object;
            if (matchVariableName(name, "from", 4) && IS_INT(value)) range->from = AS_INT(value);
            else if (matchVariableName(name, "to", 4) && IS_INT(value)) range->to = AS_INT(value);
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);

            push(vm, value);
            return true;
        }
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            if (matchVariableName(name, "length", 6)) {
                runtimeError(vm, "Cannot set property length on Object String.");
                return false;
            }
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);
        }
        case OBJ_TIMER: {
            ObjTimer* timer = (ObjTimer*)object;
            if (matchVariableName(name, "id", 2) && IS_INT(value)) timer->id = AS_INT(value);
            else if (matchVariableName(name, "isRunning", 9)) timer->isRunning = AS_BOOL(value);
            else return setAndPushGenericInstanceVariableByName(vm, object, name, value);

            push(vm, value);
            return true;
        }
        default:
            runtimeError(vm, "Undefined property %s on Object type %d.", name->chars, object->type);
            return false;
    }
}

static bool setGenericInstanceVariable(VM* vm, Obj* object, Chunk* chunk, uint8_t byte, Value value) {
    InlineCache* inlineCache = &chunk->inlineCaches[byte];
    int shapeID = object->shapeID;
    if (inlineCache->type == CACHE_IVAR && inlineCache->id == shapeID) {
#ifdef DEBUG_TRACE_CACHE
        printf("Cache hit for setting instance variable: Shape ID %d at index %d.\n", inlineCache->id, inlineCache->index);
#endif 
        return setGenericInstanceVariableByIndex(vm, object, inlineCache->index, value);
    }

#ifdef DEBUG_TRACE_CACHE
    printf("Cache miss for setting instance variable: Shape ID %d.\n", shapeID);
#endif

    ObjString* name = AS_STRING(chunk->identifiers.values[byte]);
    IDMap* idMap = getShapeIndexes(vm, shapeID);
    int index;
    if (idMapGet(idMap, name, &index)) {
        writeInlineCache(inlineCache, CACHE_IVAR, shapeID, index);
        return setGenericInstanceVariableByIndex(vm, object, index, value);
    }
    return setGenericInstanceVariableByName(vm, object, name, value);
}

bool setInstanceVariable(VM* vm, Value receiver, Chunk* chunk, uint8_t byte, Value value) {
    InlineCache* inlineCache = &chunk->inlineCaches[byte];
    if (IS_INSTANCE(receiver)) {
        ObjInstance* instance = AS_INSTANCE(receiver);
        int shapeID = instance->obj.shapeID;
        if (inlineCache->type == CACHE_IVAR && inlineCache->id == shapeID) {
#ifdef DEBUG_TRACE_CACHE
            printf("Cache hit for setting instance variable: Shape ID %d at index %d.\n", inlineCache->id, inlineCache->index);
#endif 
            instance->fields.values[inlineCache->index] = value;
            push(vm, value);
            return true;
        }

#ifdef DEBUG_TRACE_CACHE
        printf("Cache miss for setting instance variable: Shape ID %d.\n", shapeID);
#endif

        ObjString* name = AS_STRING(chunk->identifiers.values[byte]);
        IDMap* idMap = getShapeIndexes(vm, shapeID);
        int index;
        if (idMapGet(idMap, name, &index)) instance->fields.values[index] = value;
        else {
            index = instance->fields.count;
            transitionShapeForObject(vm, &instance->obj, name);
            valueArrayWrite(vm, &instance->fields, value);
            shapeID = instance->obj.shapeID;
        }

        writeInlineCache(inlineCache, CACHE_IVAR, shapeID, index);
        push(vm, value);
        return true;
    }
    else if (IS_CLASS(receiver)) {
        ObjClass* klass = AS_CLASS(receiver);
        if (inlineCache->type == CACHE_CVAR && inlineCache->id == klass->behaviorID) {
#ifdef DEBUG_TRACE_CACHE
            printf("Cache hit for setting class variable: Behavior ID %d at index %d.\n", inlineCache->id, inlineCache->index);
#endif 

            klass->fields.values[inlineCache->index] = value;
            push(vm, value);
            return true;
        }

#ifdef DEBUG_TRACE_CACHE
        printf("Cache miss for setting class variable: Behavior ID %d.\n", klass->behaviorID);
#endif

        ObjString* name = AS_STRING(chunk->identifiers.values[byte]);
        int index;
        if (idMapGet(&klass->indexes, name, &index)) klass->fields.values[index] = value;
        else {
            index = klass->fields.count;
            idMapSet(vm, &klass->indexes, name, index);
            valueArrayWrite(vm, &klass->fields, value);
        }

        writeInlineCache(inlineCache, CACHE_CVAR, klass->behaviorID, index);
        push(vm, value);
        return true;
    }
    else if (IS_NAMESPACE(receiver)) { 
        ObjNamespace* namespace = AS_NAMESPACE(receiver);
        if (!IS_CLASS(value) && !IS_NAMESPACE(value)) { 
            runtimeError(vm, "Only classes, traits and sub-namespaces may be assigned to namespace %s.", namespace->fullName->chars);
            return false;
        }

        ObjString* name = AS_STRING(chunk->identifiers.values[byte]);
        Value existingValue; 
        if (tableGet(&namespace->values, name, &existingValue)) {
            runtimeError(vm, "Identifier %s already exists as class, trait or subnamespace in namespace %s", name->chars, namespace->fullName->chars);
            return false;
        }

        tableSet(vm, &namespace->values, name, value);
        return true;
    }
    else if (IS_OBJ(receiver)) {
        return setGenericInstanceVariable(vm, AS_OBJ(receiver), chunk, byte, value);
    }
    else {
        runtimeError(vm, "Only instances and classes can set properties.");
        return false;
    }
}

int getOffsetForGenericObject(Obj* object) {
    switch (object->type) { 
        case OBJ_ARRAY: return 1; 
        case OBJ_BOUND_METHOD: return 2;
        case OBJ_CLOSURE: return 2;
        case OBJ_DICTIONARY: return 1;
        case OBJ_ENTRY: return 2;
        case OBJ_EXCEPTION: return 2;
        case OBJ_FILE: return 2;
        case OBJ_GENERATOR: return 3;
        case OBJ_METHOD: return 3;
        case OBJ_NODE: return 3;
        case OBJ_PROMISE: return 3;
        case OBJ_RANGE: return 2;
        case OBJ_STRING: return 1;
        case OBJ_TIMER: return 2;
        default: return 0;
    }
}
