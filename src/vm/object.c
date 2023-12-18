#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"

Obj* allocateObject(VM* vm, size_t size, ObjType type, ObjClass* klass) {
    Obj* object = (Obj*)reallocate(vm, NULL, 0, size);
    object->objectID = 0;
    object->shapeID = 0;
    object->type = type;
    object->klass = klass;
    object->isMarked = false;

    object->next = vm->objects;
    vm->objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

ObjArray* newArray(VM* vm) {
    ObjArray* array = ALLOCATE_OBJ(ObjArray, OBJ_ARRAY, vm->arrayClass);
    initValueArray(&array->elements);
    return array;
}

ObjBoundMethod* newBoundMethod(VM* vm, Value receiver, ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD, vm->boundMethodClass);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjClass* newClass(VM* vm, ObjString* name) {
    if (vm->behaviorCount == INT32_MAX) {
        runtimeError(vm, "Cannot have more than %d classes/traits.", INT32_MAX);
        return NULL;
    }

    ObjString* metaclassName = formattedString(vm, "%s class", name->chars);
    ObjClass* metaclass = createClass(vm, metaclassName, vm->metaclassClass, BEHAVIOR_METACLASS);
    push(vm, OBJ_VAL(metaclass));
    ObjClass* klass = createClass(vm, name, metaclass, BEHAVIOR_CLASS);
    pop(vm);
    return klass;
}

ObjClosure* newClosure(VM* vm, ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE, vm->functionClass);
    closure->function = function;
    closure->module = vm->currentModule;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjDictionary* newDictionary(VM* vm) {
    ObjDictionary* dict = ALLOCATE_OBJ(ObjDictionary, OBJ_DICTIONARY, vm->dictionaryClass);
    dict->count = 0;
    dict->capacity = 0;
    dict->entries = NULL;
    return dict;
}

ObjEntry* newEntry(VM* vm, Value key, Value value) {
    ObjEntry* entry = ALLOCATE_OBJ(ObjEntry, OBJ_ENTRY, vm->entryClass);
    entry->key = key;
    entry->value = value;
    return entry;
}

ObjFile* newFile(VM* vm, ObjString* name) {
    ObjFile* file = ALLOCATE_OBJ(ObjFile, OBJ_FILE, vm->fileClass);
    file->name = name;
    file->mode = emptyString(vm);
    file->isOpen = false;
    return file;
}

ObjFunction* newFunction(VM* vm) {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION, NULL);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjInstance* newInstance(VM* vm, ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE, klass);
    initValueArray(&instance->fields);
    return instance;
}

ObjMethod* newMethod(VM* vm, ObjClass* behavior, ObjClosure* closure) {
    ObjMethod* method = ALLOCATE_OBJ(ObjMethod, OBJ_METHOD, vm->methodClass);
    method->behavior = behavior;
    method->closure = closure;
    return method;
}

ObjModule* newModule(VM* vm, ObjString* path) {
    ObjModule* module = ALLOCATE_OBJ(ObjModule, OBJ_MODULE, NULL);
    module->path = path;
    module->isNative = false;
    initIDMap(&module->valIndexes);
    initValueArray(&module->valFields);
    initIDMap(&module->varIndexes);
    initValueArray(&module->varFields);

    for (int i = 0; i < vm->langNamespace->values.capacity; i++) {
        Entry* entry = &vm->langNamespace->values.entries[i];
        if (entry->key != NULL) {
            idMapSet(vm, &module->valIndexes, entry->key, module->valFields.count);
            valueArrayWrite(vm, &module->valFields, entry->value);
        }
    }
    tableSet(vm, &vm->modules, path, NIL_VAL);
    return module;
}

ObjNamespace* newNamespace(VM* vm, ObjString* shortName, ObjNamespace* enclosing) {
    ObjNamespace* namespace = ALLOCATE_OBJ(ObjNamespace, OBJ_NAMESPACE, vm->namespaceClass);
    namespace->shortName = shortName;
    namespace->enclosing = enclosing;
    namespace->isRoot = false;

    if (namespace->enclosing != NULL && !namespace->enclosing->isRoot) { 
        char chars[UINT8_MAX];
        int length = sprintf_s(chars, UINT8_MAX, "%s.%s", namespace->enclosing->fullName->chars, shortName->chars);
        namespace->fullName = copyString(vm, chars, length);
    } 
    else namespace->fullName = namespace->shortName;

    initTable(&namespace->values);
    return namespace;
}

ObjNativeFunction* newNativeFunction(VM* vm, ObjString* name, int arity, NativeFunction function) {
    ObjNativeFunction* nativeFunction = ALLOCATE_OBJ(ObjNativeFunction, OBJ_NATIVE_FUNCTION, vm->functionClass);
    nativeFunction->name = name;
    nativeFunction->arity = arity;
    nativeFunction->function = function;
    return nativeFunction;
}

ObjNativeMethod* newNativeMethod(VM* vm, ObjClass* klass, ObjString* name, int arity, NativeMethod method) {
    ObjNativeMethod* nativeMethod = ALLOCATE_OBJ(ObjNativeMethod, OBJ_NATIVE_METHOD, vm->methodClass);
    nativeMethod->klass = klass;
    nativeMethod->name = name;
    nativeMethod->arity = arity;
    nativeMethod->method = method;
    return nativeMethod;
}

ObjNode* newNode(VM* vm, Value element, ObjNode* prev, ObjNode* next) {
    ObjNode* node = ALLOCATE_OBJ(ObjNode, OBJ_NODE, vm->nodeClass);
    node->element = element;
    node->prev = prev;
    node->next = next;
    return node;
}

ObjRange* newRange(VM* vm, int from, int to) {
    ObjRange* range = ALLOCATE_OBJ(ObjRange, OBJ_RANGE, vm->rangeClass);
    range->from = from;
    range->to = to;
    return range;
}

ObjRecord* newRecord(VM* vm, void* data) {
    ObjRecord* record = ALLOCATE_OBJ(ObjRecord, OBJ_RECORD, NULL);
    record->data = data;
    record->markFunction = NULL;
    record->freeFunction = NULL;
    return record;
}

ObjUpvalue* newUpvalue(VM* vm, Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE, NULL);
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

Value getObjProperty(VM* vm, ObjInstance* object, char* name) {
    IDMap* idMap = getShapeIndexes(vm, object->obj.shapeID);
    int index;
    idMapGet(idMap, newString(vm, name), &index);
    return object->fields.values[index];
}

void setObjProperty(VM* vm, ObjInstance* object, char* name, Value value) {
    IDMap* idMap = getShapeIndexes(vm, object->obj.shapeID);
    ObjString* key = newString(vm, name);
    int index;
    push(vm, OBJ_VAL(key));

    if (idMapGet(idMap, key, &index)) object->fields.values[index] = value;
    else {
        transitionShapeForObject(vm, &object->obj, key);
        valueArrayWrite(vm, &object->fields, value);
    }
    pop(vm);
}

void copyObjProperty(VM* vm, ObjInstance* fromObject, ObjInstance* toObject, char* name) {
    Value value = getObjProperty(vm, fromObject, name);
    setObjProperty(vm, toObject, name, value);
}

void copyObjProperties(VM* vm, ObjInstance* fromObject, ObjInstance* toObject) {
    toObject->obj.shapeID = fromObject->obj.shapeID;
    for (int i = 0; i < fromObject->fields.count; i++) {
        valueArrayWrite(vm, &toObject->fields, fromObject->fields.values[i]);
    }
}

Value getClassProperty(VM* vm, ObjClass* klass, char* name) {
    int index;
    if (!idMapGet(&klass->indexes, newString(vm, name), &index)) {
        runtimeError(vm, "Class property %s does not exist for class %s", name, klass->fullName->chars);
        return NIL_VAL;
    }
    return klass->fields.values[index];
}

Value getObjMethod(VM* vm, Value object, char* name) {
    ObjClass* klass = getObjClass(vm, object);
    Value method;
    if (!tableGet(&klass->methods, newString(vm, name), &method)) {
        runtimeError(vm, "Method %s::%s does not exist.", klass->name->chars, name);
    }
    return method;
}

static void printArray(ObjArray* array) {
    printf("[");
    for (int i = 0; i < array->elements.count; i++) {
        printValue(array->elements.values[i]);
        if (i < array->elements.count - 1) printf(", ");
    }
    printf("]");
}

static void printClass(ObjClass* klass) {
    switch (klass->behavior) {
        case BEHAVIOR_METACLASS:
            printf("<metaclass %s>", klass->name->chars);
            break;
        case BEHAVIOR_TRAIT:
            printf("<trait %s>", klass->name->chars);
            break;
        default:
            printf("<class %s>", klass->name->chars);
    }
}

static void printDictionary(ObjDictionary* dictionary) {
    printf("[");
    int startIndex = 0;
    for (int i = 0; i < dictionary->capacity; i++) {
        ObjEntry* entry = &dictionary->entries[i];
        if (IS_UNDEFINED(entry->key)) continue;
        printValue(entry->key);
        printf(": ");
        printValue(entry->value);
        startIndex = i + 1;
        break;
    }

    for (int i = startIndex; i < dictionary->capacity; i++) {
        ObjEntry* entry = &dictionary->entries[i];
        if (IS_UNDEFINED(entry->key)) continue;
        printf(", ");
        printValue(entry->key);
        printf(": ");
        printValue(entry->value);
    }
    printf("]");
}

static void printFunction(ObjFunction* function) {
    if (function->name == NULL) {
        printf("<script>");
    }
    else if (function->name->length == 0) {
        printf("<function>");
    }
    else printf("<function %s>", function->name->chars);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_ARRAY:
            printArray(AS_ARRAY(value));
            break;
        case OBJ_BOUND_METHOD:
            printf("<bound method %s::%s>", AS_OBJ(AS_BOUND_METHOD(value)->receiver)->klass->name->chars, AS_BOUND_METHOD(value)->method->function->name->chars);
            break;
        case OBJ_CLASS:
            printClass(AS_CLASS(value));
            break;
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
        case OBJ_DICTIONARY:
            printDictionary(AS_DICTIONARY(value));
            break;
        case OBJ_ENTRY:
            printf("<entry>");
            break;
        case OBJ_FILE:
            printf("<file \"%s\">", AS_FILE(value)->name->chars);
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_INSTANCE:
            printf("<object %s>", AS_OBJ(value)->klass->name->chars);
            break;
        case OBJ_METHOD:
            printf("<method %s::%s>", AS_METHOD(value)->behavior->name->chars, AS_METHOD(value)->closure->function->name->chars);
            break;
        case OBJ_NAMESPACE:
            printf("<namespace %s>", AS_NAMESPACE(value)->fullName->chars);
            break;
        case OBJ_NATIVE_FUNCTION:
            printf("<native function %s>", AS_NATIVE_FUNCTION(value)->name->chars);
            break;
        case OBJ_NATIVE_METHOD:
            printf("<native method %s::%s>", AS_NATIVE_METHOD(value)->klass->name->chars, AS_NATIVE_METHOD(value)->name->chars);
            break;
        case OBJ_NODE:
            printf("<node>");
            break;
        case OBJ_RANGE:
            printf("%d..%d", AS_RANGE(value)->from, AS_RANGE(value)->to);
            break;
        case OBJ_RECORD:
            printf("<record>");
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            printf("<upvalue>");
            break;
        default:
            printf("<unknown>");
  }
}