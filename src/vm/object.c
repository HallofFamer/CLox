#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hash.h"
#include "memory.h"
#include "native.h"
#include "object.h"
#include "string.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType, objectClass) (type*)allocateObject(vm, sizeof(type), objectType, objectClass)

Obj* allocateObject(VM* vm, size_t size, ObjType type, ObjClass* klass) {
    Obj* object = (Obj*)reallocate(vm, NULL, 0, size);
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
    initTable(&instance->fields);
    return instance;
}

ObjMethod* newMethod(VM* vm, ObjClass* behavior, ObjClosure* closure) {
    ObjMethod* method = ALLOCATE_OBJ(ObjMethod, OBJ_METHOD, vm->methodClass);
    method->behavior = behavior;
    method->closure = closure;
    return method;
}

ObjNamespace* newNamespace(VM* vm, ObjString* name, ObjString* path) {
    ObjNamespace* namespace = ALLOCATE_OBJ(ObjNamespace, OBJ_NAMESPACE, vm->namespaceClass);
    namespace->name = name;
    namespace->path = path;
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

static ObjString* createBehaviorName(VM* vm, BehaviorType behaviorType, ObjClass* superclass) {
    char hexTimestamp[16];
    int length = 16;
    _ultoa_s((unsigned long)time(NULL), hexTimestamp, length, 16);
    if (behaviorType == BEHAVIOR_TRAIT) return formattedString(vm, "Trait@%s", hexTimestamp);
    else return formattedString(vm, "%s@%s", superclass->name->chars, hexTimestamp);
}

ObjClass* createClass(VM* vm, ObjString* name, ObjClass* metaclass, BehaviorType behavior) {
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS, metaclass);
    push(vm, OBJ_VAL(klass));
    klass->name = name != NULL ? name : newString(vm, "");
    klass->behavior = behavior;
    klass->superclass = NULL;
    klass->isNative = false;
    initValueArray(&klass->traits);
    initTable(&klass->fields);
    initTable(&klass->methods);
    pop(vm);
    return klass;
}

ObjClass* createTrait(VM* vm, ObjString* name) {
    ObjClass* trait = ALLOCATE_OBJ(ObjClass, OBJ_CLASS, vm->traitClass);
    push(vm, OBJ_VAL(trait));
    trait->name = name != NULL ? name : createBehaviorName(vm, BEHAVIOR_TRAIT, NULL);
    trait->behavior = BEHAVIOR_TRAIT;
    trait->superclass = NULL;
    trait->isNative = false;
    initValueArray(&trait->traits);
    initTable(&trait->fields);
    initTable(&trait->methods);
    pop(vm);
    return trait;
}

ObjClass* getObjClass(VM* vm, Value value) {
    if (IS_BOOL(value)) return vm->boolClass;
    else if (IS_NIL(value)) return vm->nilClass;
    else if (IS_INT(value)) return vm->intClass;
    else if (IS_FLOAT(value)) return vm->floatClass;
    else if (IS_OBJ(value)) return AS_OBJ(value)->klass;
    else return NULL;
}

bool isObjInstanceOf(VM* vm, Value value, ObjClass* klass) {
    ObjClass* currentClass = getObjClass(vm, value);
    if (currentClass == klass) return true;
    if (isClassExtendingSuperclass(currentClass->superclass, klass)) return true;
    return isClassImplementingTrait(currentClass, klass);
}

bool isClassExtendingSuperclass(ObjClass* klass, ObjClass* superclass) {
    if (klass == superclass) return true;
    if (klass->behavior == BEHAVIOR_TRAIT) return false;

    ObjClass* currentClass = klass->superclass;
    while (currentClass != NULL) {
        if (currentClass == superclass) return true;
        currentClass = currentClass->superclass;
    }
    return false;
}

bool isClassImplementingTrait(ObjClass* klass, ObjClass* trait) {
    if (klass->behavior == BEHAVIOR_METACLASS || klass->traits.count == 0) return false;
    ValueArray* traits = &klass->traits;

    for (int i = 0; i < traits->count; i++) {
        if (AS_CLASS(traits->values[i]) == trait) return true;
    }
    return false;
}

void inheritSuperclass(VM* vm, ObjClass* subclass, ObjClass* superclass) {
    subclass->superclass = superclass;
    if (superclass->behavior == BEHAVIOR_CLASS) {
        for (int i = 0; i < superclass->traits.count; i++) {
            valueArrayWrite(vm, &subclass->traits, superclass->traits.values[i]);
        }
    }
    tableAddAll(vm, &superclass->methods, &subclass->methods);
}

void bindSuperclass(VM* vm, ObjClass* subclass, ObjClass* superclass) {
    if (superclass == NULL) {
        runtimeError(vm, "Superclass cannot be NULL for class %s", subclass->name);
        return;
    }
    inheritSuperclass(vm, subclass, superclass);
    if (subclass->name->length == 0) {
        subclass->name = createBehaviorName(vm, BEHAVIOR_CLASS, superclass);
        subclass->obj.klass = superclass->obj.klass;
    }
    else inheritSuperclass(vm, subclass->obj.klass, superclass->obj.klass);
}

static void copyTraitsToTable(VM* vm, ValueArray* traitArray, Table* traitTable) {
    for (int i = 0; i < traitArray->count; i++) {
        ObjClass* trait = AS_CLASS(traitArray->values[i]);
        tableSet(vm, traitTable, trait->name, traitArray->values[i]);
        if (trait->traits.count == 0) continue;

        for (int j = 0; j < trait->traits.count; j++) {
            ObjClass* superTrait = AS_CLASS(trait->traits.values[j]);
            tableSet(vm, traitTable, superTrait->name, trait->traits.values[j]);
        }
    }
}

static void copyTraitsFromTable(VM* vm, ObjClass* klass, Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;
        valueArrayWrite(vm, &klass->traits, entry->value);
    }
}

static void flattenTraits(VM* vm, ObjClass* klass, ValueArray* traits) {
    Table traitTable;
    initTable(&traitTable);

    copyTraitsToTable(vm, traits, &traitTable);
    if (klass->superclass != NULL && klass->superclass->traits.count > 0) {
        copyTraitsToTable(vm, &klass->superclass->traits, &traitTable);
    }

    freeValueArray(vm, traits);
    copyTraitsFromTable(vm, klass, &traitTable);
    freeTable(vm, &traitTable);
}

void implementTraits(VM* vm, ObjClass* klass, ValueArray* traits) {
    if (traits->count == 0) return;
    for (int i = 0; i < traits->count; i++) {
        ObjClass* trait = AS_CLASS(traits->values[i]);
        tableAddAll(vm, &trait->methods, &klass->methods);
    }
    flattenTraits(vm, klass, traits);
}

void bindTrait(VM* vm, ObjClass* klass, ObjClass* trait) {
    tableAddAll(vm, &trait->methods, &klass->methods);
    valueArrayWrite(vm, &klass->traits, OBJ_VAL(trait));
    for (int i = 0; i < trait->traits.count; i++) {
        valueArrayWrite(vm, &klass->traits, trait->traits.values[i]);
    }
}

void bindTraits(VM* vm, int numTraits, ObjClass* klass, ...) {
    va_list args;
    va_start(args, klass);
    for (int i = 0; i < numTraits; i++) {
        Value trait = va_arg(args, Value);
        bindTrait(vm, klass, AS_CLASS(trait));
    }
    flattenTraits(vm, klass, &klass->traits);
}

Value getObjProperty(VM* vm, ObjInstance* object, char* name) {
    Value value;
    tableGet(&object->fields, newString(vm, name), &value);
    return value;
}

void setObjProperty(VM* vm, ObjInstance* object, char* name, Value value) {
    ObjString* key = newString(vm, name);
    push(vm, OBJ_VAL(key));
    tableSet(vm, &object->fields, key, ((void*)value == NULL) ? NIL_VAL : value);
    pop(vm);
}

void copyObjProperty(VM* vm, ObjInstance* fromObject, ObjInstance* toObject, char* name) {
    Value value = getObjProperty(vm, fromObject, name);
    setObjProperty(vm, toObject, name, value);
}

void copyObjProperties(VM* vm, ObjInstance* fromObject, ObjInstance* toObject) {
    for (int i = 0; i < fromObject->fields.capacity; i++) {
        Entry* entry = &fromObject->fields.entries[i];
        if (entry->key == NULL) continue;
        setObjProperty(vm, toObject, entry->key->chars, entry->value);
    }
}

Value getClassProperty(VM* vm, ObjClass* klass, char* name) {
    Value value;
    tableGet(&klass->fields, newString(vm, name), &value);
    return value;
}

void setClassProperty(VM* vm, ObjClass* klass, char* name, Value value) {
    ObjString* key = newString(vm, name);
    push(vm, OBJ_VAL(key));
    tableSet(vm, &klass->fields, key, ((void*)value == NULL) ? NIL_VAL : value);
    pop(vm);
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
        case OBJ_NATIVE_FUNCTION:
            printf("<native function %s>", AS_NATIVE_FUNCTION(value)->name->chars);
            break;
        case OBJ_NATIVE_METHOD:
            printf("<native method %s::%s>", AS_NATIVE_METHOD(value)->klass->name->chars, AS_NATIVE_METHOD(value)->name->chars);
            break;
        case OBJ_RANGE:
            printf("%d..%d", AS_RANGE(value)->from, AS_RANGE(value)->to);
            break;
        case OBJ_RECORD:
            printf("record");
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
        default:
            printf("");
  }
}