#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hash.h"
#include "memory.h"
#include "object.h"
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

ObjBoundMethod* newBoundMethod(VM* vm, Value receiver, ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD, vm->methodClass);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjClass* newClass(VM* vm, ObjString* name) {
    ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS, vm->classClass);
    klass->name = name;
    klass->superclass = NULL;
    klass->isNative = false;
    initTable(&klass->methods);
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

ObjList* newList(VM* vm) {
    ObjList* list = ALLOCATE_OBJ(ObjList, OBJ_LIST, vm->listClass);
    initValueArray(&list->elements);
    return list;
}

ObjList* copyList(VM* vm, ValueArray elements, int fromIndex, int toIndex) {
    ObjList* list = ALLOCATE_OBJ(ObjList, OBJ_LIST, NULL);
    initValueArray(&list->elements);
    for (int i = fromIndex; i < toIndex; i++) {
        writeValueArray(vm, &list->elements, elements.values[i]);
    }
    return list;
}

ObjNativeFunction* newNativeFunction(VM* vm, ObjString* name, int arity, NativeFn function) {
    ObjNativeFunction* nativeFunction = ALLOCATE_OBJ(ObjNativeFunction, OBJ_NATIVE_FUNCTION, vm->functionClass);
    nativeFunction->name = name;
    nativeFunction->arity = arity;
    nativeFunction->function = function;
    return nativeFunction;
}

ObjNativeMethod* newNativeMethod(VM* vm, ObjClass* klass, ObjString* name, int arity, NativeMethod method) {
    ObjNativeMethod* nativeMethod = ALLOCATE_OBJ(ObjNativeMethod, OBJ_NATIVE_METHOD, NULL);
    nativeMethod->klass = klass;
    nativeMethod->name = name;
    nativeMethod->arity = arity;
    nativeMethod->method = method;
    return nativeMethod;
}

ObjUpvalue* newUpvalue(VM* vm, Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE, NULL);
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

ObjClass* getObjClass(VM* vm, Value value) {
    if (IS_BOOL(value)) return vm->boolClass;
    else if (IS_NIL(value)) return vm->nilClass;
    else if (IS_INT(value)) return vm->intClass;
    else if (IS_FLOAT(value)) return vm->floatClass;
    else if (IS_OBJ(value)) return AS_OBJ(value)->klass;
    else return NULL;
}

static void printFunction(ObjFunction* function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

static void printList(ObjList* list) {
    printf("[");
    for (int i = 0; i < list->elements.count; i++) {
        printValue(list->elements.values[i]);
        if (i < list->elements.count - 1) printf(", ");
    }
    printf("]");
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD:
            printFunction(AS_BOUND_METHOD(value)->method->function);
            break;
        case OBJ_CLASS:
            printf("<class %s>", AS_CLASS(value)->name->chars);
            break;
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_INSTANCE:
            printf("<object %s>", AS_OBJ(value)->klass->name->chars);
            break;
        case OBJ_LIST: 
            printList(AS_LIST(value));
            break;
        case OBJ_NATIVE_FUNCTION:
            printf("<native function>");
            break;
        case OBJ_NATIVE_METHOD:
            printf("<native method>");
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
  }
}