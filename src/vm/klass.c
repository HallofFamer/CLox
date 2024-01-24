#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "klass.h"
#include "vm.h"

static ObjString* createBehaviorName(VM* vm, BehaviorType behaviorType, ObjClass* superclass) {
    unsigned long currentTimeStamp = (unsigned long)time(NULL);
    if (behaviorType == BEHAVIOR_TRAIT) return formattedString(vm, "Trait@%x", currentTimeStamp);
    else return formattedString(vm, "%s@%x", superclass->name->chars, currentTimeStamp);
}

void initClass(VM* vm, ObjClass* klass, ObjString* name, ObjClass* metaclass, BehaviorType behaviorType) {
    push(vm, OBJ_VAL(klass));
    klass->behaviorID = vm->behaviorCount++;
    klass->behaviorType = behaviorType;
    klass->classType = OBJ_INSTANCE;
    klass->name = name != NULL ? name : emptyString(vm);
    klass->namespace = vm->currentNamespace;
    klass->superclass = NULL;
    klass->isNative = false;
    klass->interceptors = 0;

    if (!klass->namespace->isRoot) {
        char chars[UINT8_MAX];
        int length = sprintf_s(chars, UINT8_MAX, "%s.%s", klass->namespace->fullName->chars, klass->name->chars);
        klass->fullName = copyString(vm, chars, length);
    }
    else klass->fullName = klass->name;

    initValueArray(&klass->traits);
    initIDMap(&klass->indexes);
    initValueArray(&klass->fields);
    initTable(&klass->methods);
    pop(vm);
}

ObjClass* createClass(VM* vm, ObjString* name, ObjClass* metaclass, BehaviorType behaviorType) {
    ObjClass* klass = ALLOCATE_CLASS(metaclass);
    initClass(vm, klass, name, metaclass, behaviorType);
    return klass;
}

void initTrait(VM* vm, ObjClass* trait, ObjString* name) {
    push(vm, OBJ_VAL(trait));
    trait->behaviorType = BEHAVIOR_TRAIT;
    trait->behaviorID = vm->behaviorCount++;
    trait->name = name != NULL ? name : createBehaviorName(vm, BEHAVIOR_TRAIT, NULL);
    trait->namespace = vm->currentNamespace;
    trait->superclass = NULL;
    trait->isNative = false;
    trait->interceptors = 0;

    if (!trait->namespace->isRoot) {
        char chars[UINT8_MAX];
        int length = sprintf_s(chars, UINT8_MAX, "%s.%s", trait->namespace->fullName->chars, trait->name->chars);
        trait->fullName = copyString(vm, chars, length);
    }
    else trait->fullName = trait->name;

    initValueArray(&trait->traits);
    initIDMap(&trait->indexes);
    initValueArray(&trait->fields);
    initTable(&trait->methods);
    pop(vm);
}

ObjClass* createTrait(VM* vm, ObjString* name) {
    ObjClass* trait = ALLOCATE_CLASS(vm->traitClass);
    initTrait(vm, trait, name);
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
    if (klass->behaviorType == BEHAVIOR_TRAIT) return false;

    ObjClass* currentClass = klass->superclass;
    while (currentClass != NULL) {
        if (currentClass == superclass) return true;
        currentClass = currentClass->superclass;
    }
    return false;
}

bool isClassImplementingTrait(ObjClass* klass, ObjClass* trait) {
    if (klass->behaviorType == BEHAVIOR_METACLASS || klass->traits.count == 0) return false;
    ValueArray* traits = &klass->traits;

    for (int i = 0; i < traits->count; i++) {
        if (AS_CLASS(traits->values[i]) == trait) return true;
    }
    return false;
}

void inheritSuperclass(VM* vm, ObjClass* subclass, ObjClass* superclass) {
    subclass->superclass = superclass;
    subclass->classType = superclass->classType;
    if (superclass->behaviorType == BEHAVIOR_CLASS) {
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

void setClassProperty(VM* vm, ObjClass* klass, char* name, Value value) {
    ObjString* propertyName = newString(vm, name);
    int index;
    if (!idMapGet(&klass->indexes, propertyName, &index)) {
        index = klass->fields.count;
        valueArrayWrite(vm, &klass->fields, value);
        idMapSet(vm, &klass->indexes, propertyName, index);
    }
    else klass->fields.values[index] = value;
}