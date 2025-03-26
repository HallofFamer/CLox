#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "typechecker.h"
#include "../vm/native.h"
#include "../vm/vm.h"

struct ClassTypeChecker {
    ClassTypeChecker* enclosing;
    Token name;
    BehaviorTypeInfo* type;
    bool isAnonymous;
};

struct FunctionTypeChecker {
    FunctionTypeChecker* enclosing;
    Token name;
    SymbolTable* symtab;
    CallableTypeInfo* type;
    bool isAsync;
    bool isClass;
};

static void typeError(TypeChecker* typeChecker, const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[line %d] Type Error: ", typeChecker->currentToken.line);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);
    typeChecker->hadError = true;
}

static void initClassTypeChecker(TypeChecker* typeChecker, ClassTypeChecker* klass, Token name, BehaviorTypeInfo* type, bool isAnonymous) {
    klass->enclosing = typeChecker->currentClass;
    klass->name = name;
    klass->type = type;
    klass->isAnonymous = isAnonymous;
    typeChecker->currentClass = klass;
}

static void endClassTypeChecker(TypeChecker* typeChecker) {
    typeChecker->currentClass = typeChecker->currentClass->enclosing;
}

static void initFunctionTypeChecker(TypeChecker* typeChecker, FunctionTypeChecker* function, Token name, CallableTypeInfo* type, bool isAsync, bool isClass) {
    function->enclosing = typeChecker->currentFunction;
    function->name = name;
    function->type = type;
    function->isAsync = isAsync;
    function->isClass = isClass;
    typeChecker->currentFunction = function;
}

static void endFunctionTypeChecker(TypeChecker* typeChecker) {
    typeChecker->currentFunction = typeChecker->currentFunction->enclosing;
}

void initTypeChecker(VM* vm, TypeChecker* typeChecker, bool debugTypetab) {
    typeChecker->vm = vm;
    typeChecker->currentNamespace = emptyString(vm);
    typeChecker->currentClass = NULL;
    typeChecker->currentFunction = NULL;

    typeChecker->objectType = getNativeType(vm, "Object");
    typeChecker->nilType = getNativeType(vm, "Nil");
    typeChecker->boolType = getNativeType(vm, "Bool");
    typeChecker->numberType = getNativeType(vm, "Number");
    typeChecker->intType = getNativeType(vm, "Int");
    typeChecker->stringType = getNativeType(vm, "String");
    typeChecker->classType = getNativeType(vm, "Class");
    typeChecker->functionType = getNativeType(vm, "Function");
    typeChecker->voidType = getNativeType(vm, "void");

    typeChecker->debugTypetab = debugTypetab;
    typeChecker->hadError = false;
}

static ObjString* createSymbol(TypeChecker* typeChecker, Token token) {
    return copyString(typeChecker->vm, token.start, token.length);
}

static TypeInfo* getClassType(TypeChecker* typeChecker, ObjString* shortName, SymbolTable* symtab) {
    if (shortName == NULL) return NULL;
    TypeInfo* type = typeTableGet(typeChecker->vm->typetab, shortName);

    if (type == NULL) {
        ObjString* fullName = concatenateString(typeChecker->vm, typeChecker->currentNamespace, shortName, ".");
        type = typeTableGet(typeChecker->vm->typetab, fullName);

        if (type == NULL) {
            fullName = concatenateString(typeChecker->vm, typeChecker->vm->langNamespace->fullName, shortName, ".");
            type = typeTableGet(typeChecker->vm->typetab, fullName);

            if (type == NULL) {
                SymbolItem* item = symbolTableLookup(symtab, shortName);
                if (item != NULL && item->type != NULL) {
                    type = typeTableGet(typeChecker->vm->typetab, getClassNameFromMetaclass(typeChecker->vm, item->type->fullName));
                }
            }
        }
    }
    return type;
}

static void defineAstType(TypeChecker* typeChecker, Ast* ast, const char* name, SymbolItem* item) {
    ObjString* typeName = newString(typeChecker->vm, name);
    ast->type = getNativeType(typeChecker->vm, name);
    if (item != NULL) item->type = ast->type;
}

static bool hasAstType(TypeChecker* typeChecker, Ast* ast, const char* name) {
    ObjString* typeName = newString(typeChecker->vm, name);
    TypeInfo* type = typeTableGet(typeChecker->vm->typetab, typeName);
    return isSubtypeOfType(ast->type, type);
}

static void checkArguments(TypeChecker* typeChecker, const char* calleeDesc, Ast* ast, CallableTypeInfo* callableType) {
    if (!callableType->modifier.isVariadic) {
        if (callableType->paramTypes->count != ast->children->count) {
            typeError(typeChecker, "%s expects to receive a total of %d arguments but gets %d.", 
                calleeDesc, callableType->paramTypes->count, ast->children->count);
            return;
        }

        for (int i = 0; i < callableType->paramTypes->count; i++) {
            TypeInfo* paramType = callableType->paramTypes->elements[i];
            TypeInfo* argType = ast->children->elements[i]->type;
            if (!isSubtypeOfType(argType, paramType)) {
                typeError(typeChecker, "%s expects argument %d to be an instance of %s but gets %s.", 
                    calleeDesc, i + 1, paramType->shortName->chars, argType->shortName->chars);
            }
        }
    }
    else {
        TypeInfo* paramType = callableType->paramTypes->elements[0];
        for (int i = 0; i < ast->children->count; i++) {
            TypeInfo* argType = ast->children->elements[i]->type;
            if (!isSubtypeOfType(argType, paramType)) {
                typeError(typeChecker, "%s expects variadic arguments to be an instance of %s but gets %s.",
                    calleeDesc, paramType->shortName->chars, argType->shortName->chars);
            }
        }
    }
}

static void checkMethodSignatures(TypeChecker* typeChecker, CallableTypeInfo* subclassMethod, CallableTypeInfo* superclassMethod) {
    ObjString* className = createSymbol(typeChecker, typeChecker->currentClass->name);
    if (!subclassMethod->modifier.isVoid && superclassMethod->modifier.isVoid) {
        typeError(typeChecker, "Method %s::%s expects return type to be void.", className->chars, subclassMethod->baseType.shortName->chars);
    }
    else if (!isEqualType(subclassMethod->returnType, superclassMethod->returnType)) {
        typeError(typeChecker, "Method %s::%s expects return type to be an instance of %s but gets %s.", className->chars,
            subclassMethod->baseType.shortName->chars, superclassMethod->returnType->shortName->chars, subclassMethod->returnType->shortName->chars);
    }

    if (subclassMethod->paramTypes->count != superclassMethod->paramTypes->count) {
        typeError(typeChecker, "Method %s::%s expects to receive %d parameters but gets %d.", className->chars,
            subclassMethod->baseType.shortName->chars, superclassMethod->paramTypes->count, subclassMethod->paramTypes->count);
    }
    else {
        for (int i = 0; i < subclassMethod->paramTypes->count; i++) {
            if (!isEqualType(subclassMethod->paramTypes->elements[i], superclassMethod->paramTypes->elements[i])) {
                typeError(typeChecker, "Method %s::%s expects argument %d to be %s but gets %s.", className->chars, subclassMethod->baseType.shortName->chars,
                    i + 1, superclassMethod->paramTypes->elements[i]->shortName->chars, subclassMethod->paramTypes->elements[i]->shortName->chars);
            }
        }
    }
}

static void checkInheritingSuperclass(TypeChecker* typeChecker, TypeInfo* superclassBaseType) {
    if (typeChecker->currentClass->type == NULL || superclassBaseType == NULL) return;
    BehaviorTypeInfo* superclassType = AS_BEHAVIOR_TYPE(superclassBaseType);

    for (int i = 0; i < superclassType->methods->capacity; i++) {
        TypeEntry* methodEntry = &superclassType->methods->entries[i];
        if (methodEntry == NULL || methodEntry->key == NULL) continue;
        CallableTypeInfo* methodType = AS_CALLABLE_TYPE(methodEntry->value);

        TypeInfo* subclassMethodType = typeTableGet(typeChecker->currentClass->type->methods, methodEntry->key);
        if (subclassMethodType != NULL && subclassMethodType->shortName != typeChecker->vm->initString) {
            checkMethodSignatures(typeChecker, AS_CALLABLE_TYPE(subclassMethodType), methodType);
        }
    }

    checkInheritingSuperclass(typeChecker, superclassType->superclassType);
}

static void checkImplementingTraits(TypeChecker* typeChecker, Ast* traitList) {
    if (!astHasChild(traitList) || typeChecker->currentClass->type == NULL) return;
    BehaviorTypeInfo* superclassType = (typeChecker->currentClass->type->superclassType != NULL) ? AS_BEHAVIOR_TYPE(typeChecker->currentClass->type->superclassType) : NULL;
    if (superclassType == NULL) return;

    for (int i = 0; i < traitList->children->count; i++) {
        Ast* trait = astGetChild(traitList, 0);
        ObjString* name = createSymbol(typeChecker, trait->token);
        TypeInfo* type = getClassType(typeChecker, name, traitList->symtab);
        if (type == NULL) continue;
        else {
            BehaviorTypeInfo* traitType = AS_BEHAVIOR_TYPE(type);
            for (int j = 0; j < traitType->methods->capacity; j++) {
                TypeEntry* methodEntry = &traitType->methods->entries[i];
                if (methodEntry == NULL || methodEntry->key == NULL) continue;
                CallableTypeInfo* methodType = AS_CALLABLE_TYPE(methodEntry->value);

                TypeInfo* subclassMethodType = typeTableGet(typeChecker->currentClass->type->methods, methodEntry->key);
                if (subclassMethodType != NULL && subclassMethodType->shortName != typeChecker->vm->initString) {
                    checkMethodSignatures(typeChecker, AS_CALLABLE_TYPE(subclassMethodType), methodType);
                }

                TypeInfo* superclassMethodType = typeTableGet(superclassType->methods, methodEntry->key);
                if (superclassMethodType != NULL && superclassMethodType->shortName != typeChecker->vm->initString) {
                    checkMethodSignatures(typeChecker, methodType, AS_CALLABLE_TYPE(superclassMethodType));
                }
            }
        }
    }
}

static void inferAstTypeFromChild(Ast* ast, int childIndex, SymbolItem* item) {
    Ast* child = astGetChild(ast, childIndex);
    ast->type = child->type;
    if (item != NULL) item->type = ast->type;
}

static void inferAstTypeFromUnary(TypeChecker* typeChecker, Ast* ast, SymbolItem* item) {
    Ast* child = astGetChild(ast, 0);
    if (child->type == NULL) return;

    switch (ast->token.type) {
        case TOKEN_BANG:
            defineAstType(typeChecker, ast, "Bool", item);
            break;
        case TOKEN_MINUS:
            if (!isSubtypeOfType(child->type, typeChecker->numberType)) {
                typeError(typeChecker, "Unary negate expects operand to be an instance of Number, %s given.", child->type->shortName->chars);
            }
            else if (isSubtypeOfType(child->type, typeChecker->intType)) {
                defineAstType(typeChecker, ast, "Int", item);
            }
            else {
                defineAstType(typeChecker, ast, "Number", item);
            }
            break;
        default:
            break;
    }
}

static void inferAstTypeFromBinaryOperator(TypeChecker* typeChecker, Ast* ast, SymbolItem* item) {
    Ast* receiver = astGetChild(ast, 0);
    Ast* arg = astGetChild(ast, 1);
    if (receiver->type == NULL || arg->type == NULL) return;

    ObjString* methodName = copyString(typeChecker->vm, ast->token.start, ast->token.length);
    TypeInfo* baseType = typeTableMethodLookup(receiver->type, methodName);
    if (baseType == NULL) return;

    CallableTypeInfo* methodType = AS_CALLABLE_TYPE(baseType);
    if (methodType->paramTypes->count == 0) return;
    TypeInfo* paramType = methodType->paramTypes->elements[0];

    if (!isSubtypeOfType(arg->type, paramType)) {
        typeError(typeChecker, "Method %s::%s expects argument 0 to be an instance of %s but gets %s.",
            receiver->type->shortName->chars, methodName->chars, paramType->shortName->chars, arg->type->shortName->chars);
    }
    ast->type = methodType->returnType;
}

static void inferAstTypeFromBinary(TypeChecker* typeChecker, Ast* ast, SymbolItem* item) {
    Ast* left = astGetChild(ast, 0);
    Ast* right = astGetChild(ast, 1);
    if (left->type == NULL || right->type == NULL) return;

    switch (ast->token.type) {
        case TOKEN_PLUS:
            if (isSubtypeOfType(left->type, typeChecker->stringType) && isSubtypeOfType(right->type, typeChecker->stringType)) {
                defineAstType(typeChecker, ast, "String", item);
                return;
            }
            else if (isSubtypeOfType(left->type, typeChecker->intType) && isSubtypeOfType(right->type, typeChecker->intType)) {
                defineAstType(typeChecker, ast, "Int", item);
                return;
            }
            else if (isSubtypeOfType(left->type, typeChecker->numberType) && isSubtypeOfType(right->type, typeChecker->numberType)) {
                defineAstType(typeChecker, ast, "Number", item);
                return;
            }
            break;
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_MODULO:
            if (isSubtypeOfType(left->type, typeChecker->intType) && isSubtypeOfType(right->type, typeChecker->intType)) {
                defineAstType(typeChecker, ast, "Int", item);
                return;
            }
            else if (isSubtypeOfType(left->type, typeChecker->numberType) && isSubtypeOfType(right->type, typeChecker->numberType)) {
                defineAstType(typeChecker, ast, "Number", item);
                return;
            }
            break;
        case TOKEN_SLASH:
            if (isSubtypeOfType(left->type, typeChecker->numberType) && isSubtypeOfType(right->type, typeChecker->numberType)) {
                defineAstType(typeChecker, ast, "Number", item);
                return;
            }
            break;
        case TOKEN_DOT_DOT:
            if (isSubtypeOfType(left->type, typeChecker->intType) && isSubtypeOfType(right->type, typeChecker->intType)) {
                defineAstType(typeChecker, ast, "clox.std.collection.Range", item);
                return;
            }
            break;
        default:
            break;
    }

    inferAstTypeFromBinaryOperator(typeChecker, ast, item);
}

static void inferAstTypeFromReturn(TypeChecker* typeChecker, Ast* ast, CallableTypeInfo* callableType) {
    if (callableType == NULL || callableType->returnType == NULL) {
        return;
    }

    if (callableType->returnType->category == TYPE_CATEGORY_VOID) {
        ast->type = typeChecker->voidType;
    }
    else ast->type = callableType->returnType;
}

static void inferAstTypeFromCall(TypeChecker* typeChecker, Ast* ast) {
    Ast* callee = astGetChild(ast, 0);
    if (callee->type == NULL) return;
    Ast* args = astGetChild(ast, 1);
    ObjString* name = createSymbol(typeChecker, callee->token);
    char calleeDesc[UINT8_MAX];

    if (isSubtypeOfType(callee->type, typeChecker->functionType)) {
        CallableTypeInfo* functionType = AS_CALLABLE_TYPE(typeTableGet(typeChecker->vm->typetab, name));
        sprintf_s(calleeDesc, UINT8_MAX, "Function %s", name->chars);
        checkArguments(typeChecker, calleeDesc, args, functionType);
        inferAstTypeFromReturn(typeChecker, ast, functionType);
    }
    else if (isSubtypeOfType(callee->type, typeChecker->classType)) {
        SymbolItem* item = symbolTableGet(ast->symtab, name);
        if (item == NULL) return;
        ObjString* className = getClassNameFromMetaclass(typeChecker->vm, item->type->fullName);

        TypeInfo* classType = getClassType(typeChecker, className, ast->symtab);
        if (classType == NULL) return;
        ObjString* initializerName = newString(typeChecker->vm, "__init__");
        TypeInfo* initializerType = typeTableMethodLookup(classType, initializerName);

        if (initializerType != NULL) {
            sprintf_s(calleeDesc, UINT8_MAX, "Class %s's initializer", name->chars);
            checkArguments(typeChecker, calleeDesc, args, AS_CALLABLE_TYPE(initializerType));
        }
        else if (astHasChild(args)) {
            typeError(typeChecker, "Class %s's initializer expects to receive a total of 0 argument but gets %d.", name->chars, astNumChild(args));
        }
        ast->type = classType;
    }
}

static void inferAstTypeFromInvoke(TypeChecker* typeChecker, Ast* ast) {
    Ast* receiver = astGetChild(ast, 0);
    if (receiver->type == NULL) return;
    Ast* args = astGetChild(ast, 1);

    ObjString* methodName = createSymbol(typeChecker, ast->token);
    TypeInfo* baseType = typeTableMethodLookup(receiver->type, methodName);
    if (baseType == NULL) return;
    CallableTypeInfo* methodType = AS_CALLABLE_TYPE(baseType);

    char methodDesc[UINT8_MAX];
    sprintf_s(methodDesc, UINT8_MAX, "Method %s::%s", receiver->type->shortName->chars, methodName->chars);
    checkArguments(typeChecker, methodDesc, args, methodType);
    inferAstTypeFromReturn(typeChecker, ast, methodType);
}

static void inferAstTypeFromSuperInvoke(TypeChecker* typeChecker, Ast* ast) {
    TypeInfo* superType = (typeChecker->currentClass->type != NULL) ? typeChecker->currentClass->type->superclassType : NULL;
    if (superType == NULL) return;
    Ast* args = astGetChild(ast, 0);

    ObjString* methodName = createSymbol(typeChecker, ast->token);
    TypeInfo* baseType = typeTableMethodLookup(superType, methodName);
    if (baseType == NULL) return;
    CallableTypeInfo* methodType = AS_CALLABLE_TYPE(baseType);

    char methodDesc[UINT8_MAX];
    sprintf_s(methodDesc, UINT8_MAX, "Method %s::%s", superType->shortName->chars, methodName->chars);
    checkArguments(typeChecker, methodDesc, args, methodType);
    inferAstTypeFromReturn(typeChecker, ast, methodType);
}

static void inferAstTypeFromSubscriptGet(TypeChecker* typeChecker, Ast* ast) {
    Ast* receiver = astGetChild(ast, 0);
    Ast* index = astGetChild(ast, 1);
    if (receiver->type == NULL || index->type == NULL) return;

    if (isSubtypeOfType(receiver->type, typeChecker->stringType)) {
        if (!isSubtypeOfType(index->type, typeChecker->intType)) {
            typeError(typeChecker, "String's index must be an instance of Int but gets %s.", index->type->shortName->chars);
        }
        ast->type = typeChecker->stringType;
    }
    else if (isSubtypeOfType(receiver->type, getNativeType(typeChecker->vm, "clox.std.collection.Array"))) {
        if (!isSubtypeOfType(index->type, typeChecker->intType)) {
            typeError(typeChecker, "Array's index must be an instance of Int but gets %s.", index->type->shortName->chars);
        }
        ast->type = typeChecker->objectType;
    }
    else {
        TypeInfo* baseType = typeTableMethodLookup(receiver->type, newString(typeChecker->vm, "[]"));
        if (baseType == NULL) return;
        CallableTypeInfo* methodType = AS_CALLABLE_TYPE(baseType);
        if (methodType->paramTypes->count == 0) return;
        TypeInfo* paramType = methodType->paramTypes->elements[0];

        if (!isSubtypeOfType(index->type, paramType)) {
            typeError(typeChecker, "Method %s::[] expects argument 0 to be an instance of %s but gets %s.",
                receiver->type->shortName->chars, paramType->shortName->chars, index->type->shortName->chars);
        }
        inferAstTypeFromReturn(typeChecker, ast, methodType);
    }
}

static void inferAstTypeFromSubscriptSet(TypeChecker* typeChecker, Ast* ast) {
    Ast* receiver = astGetChild(ast, 0);
    Ast* index = astGetChild(ast, 1);
    Ast* value = astGetChild(ast, 2);
    if (receiver->type == NULL || index->type == NULL) return;

    if (isSubtypeOfType(receiver->type, typeChecker->stringType)) {
        if (!isSubtypeOfType(index->type, typeChecker->intType)) {
            typeError(typeChecker, "String's index must be an instance of Int, but gets %s.", index->type->shortName->chars);
        }
        ast->type = typeChecker->nilType;
    }
    else if (isSubtypeOfType(receiver->type, getNativeType(typeChecker->vm, "clox.std.collection.Array"))) {
        if (!isSubtypeOfType(index->type, typeChecker->intType)) {
            typeError(typeChecker, "Array's index must be an instance of Int, but gets %s.", index->type->shortName->chars);
        }
        ast->type = typeChecker->nilType;
    }
    else {
        TypeInfo* baseType = typeTableMethodLookup(receiver->type, newString(typeChecker->vm, "[]="));
        if (baseType == NULL) return;
        CallableTypeInfo* methodType = AS_CALLABLE_TYPE(baseType);
        if (methodType->paramTypes->count == 0) return;
        TypeInfo* paramType = methodType->paramTypes->elements[0];
        TypeInfo* paramType2 = methodType->paramTypes->elements[1];

        if (!isSubtypeOfType(index->type, paramType)) {
            typeError(typeChecker, "Method %s::[]= expects argument 0 to be an instance of %s but gets %s.",
                receiver->type->shortName->chars, paramType->shortName->chars, index->type->shortName->chars);
        }
        if (!isSubtypeOfType(value->type, paramType2)) {
            typeError(typeChecker, "Method %s::[]= expects argument 1 to be an instance of %s but gets %s.",
                receiver->type->shortName->chars, paramType2->shortName->chars, value->type->shortName->chars);
        }
        inferAstTypeFromReturn(typeChecker, ast, methodType);
    }
}

static void block(TypeChecker* typeChecker, Ast* ast) {
    Ast* stmts = astGetChild(ast, 0);
    for (int i = 0; i < stmts->children->count; i++) {
        typeCheckChild(typeChecker, stmts, i);
    }
}

static void function(TypeChecker* typeChecker, Ast* ast, CallableTypeInfo* calleeType, bool isAsync, bool isClass) {
    FunctionTypeChecker functionTypeChecker;
    initFunctionTypeChecker(typeChecker, &functionTypeChecker, ast->token, calleeType, isAsync, isClass);
    functionTypeChecker.symtab = ast->symtab;

    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    endFunctionTypeChecker(typeChecker);
}

static void behavior(TypeChecker* typeChecker, BehaviorType type, Ast* ast) {
    ClassTypeChecker classTypeChecker;
    ObjString* shortName = copyString(typeChecker->vm, ast->token.start, ast->token.length);
    ObjString* fullName = getClassFullName(typeChecker->vm, shortName, typeChecker->currentNamespace);
    TypeInfo* behaviorType = typeTableGet(typeChecker->vm->typetab, fullName);

    bool isAnonymous = (shortName->length == 1 && shortName->chars[0] == '@');
    initClassTypeChecker(typeChecker, &classTypeChecker, ast->token, behaviorType == NULL ? NULL : AS_BEHAVIOR_TYPE(behaviorType), isAnonymous);
    int childIndex = 0;

    if (type == BEHAVIOR_CLASS) {
        Ast* superclass = astGetChild(ast, childIndex);
        typeCheckChild(typeChecker, ast, childIndex);
        ObjString* superclassName = copyString(typeChecker->vm, superclass->token.start, superclass->token.length);
        SymbolItem* superclassItem = symbolTableLookup(ast->symtab, superclassName);
        childIndex++;

        if (!isSubtypeOfType(superclassItem->type, typeChecker->classType)) {
            typeError(typeChecker, "Superclass must be an instance of Class, but gets %s.", superclassItem->type->shortName->chars);
        }

        if (typeChecker->currentClass->isAnonymous) ast->type = superclassItem->type;
        else checkInheritingSuperclass(typeChecker, typeChecker->currentClass->type->superclassType);
    }

    Ast* traitList = astGetChild(ast, childIndex);
    if (astNumChild(traitList) > 0) {
        typeCheckChild(typeChecker, ast, childIndex);
        checkImplementingTraits(typeChecker, traitList);
    }

    childIndex++;
    typeCheckChild(typeChecker, ast, childIndex);
    endClassTypeChecker(typeChecker);
}

static void yield(TypeChecker* typeChecker, Ast* ast) {
    if (astHasChild(ast)) {
        typeCheckChild(typeChecker, ast, 0);
        Ast* child = astGetChild(ast, 0);
        if (ast->modifier.isWith && !isSubtypeOfType(child->type, getNativeType(typeChecker->vm, "Generator"))) {
            typeError(typeChecker, "'yield with' expects expression to be an instance of Generator but gets %s.", child->type->shortName->chars);
        }
    }
}

static void await(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    Ast* child = astGetChild(ast, 0);
    if (!isSubtypeOfType(child->type, getNativeType(typeChecker->vm, "clox.std.util.Promise"))) {
        typeError(typeChecker, "'await' expects expression to be an instance of Promise but gets %s.", child->type->shortName->chars);
    }
}

static void typeCheckAnd(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    defineAstType(typeChecker, ast, "Bool", NULL);
}

static void typeCheckArray(TypeChecker* typeChecker, Ast* ast) {
    if (astHasChild(ast)) {
        Ast* elements = astGetChild(ast, 0);
        for (int i = 0; i < elements->children->count; i++) {
            typeCheckChild(typeChecker, elements, i);
        }
    }
    defineAstType(typeChecker, ast, "clox.std.collection.Array", NULL);
}

static void typeCheckAssign(TypeChecker* typeChecker, Ast* ast) {
    ObjString* name = copyString(typeChecker->vm, ast->token.start, ast->token.length);
    SymbolItem* item = symbolTableLookup(ast->symtab, name);
    if (item != NULL) typeCheckChild(typeChecker, ast, 0);
    defineAstType(typeChecker, ast, "Nil", NULL);
}

static void typeCheckAwait(TypeChecker* typeChecker, Ast* ast) {
    await(typeChecker, ast);
}

static void typeCheckBinary(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    inferAstTypeFromBinary(typeChecker, ast, NULL);
}

static void typeCheckCall(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    inferAstTypeFromCall(typeChecker, ast);
}

static void typeCheckClass(TypeChecker* typeChecker, Ast* ast) {
    behavior(typeChecker, BEHAVIOR_CLASS, ast);
}

static void typeCheckDictionary(TypeChecker* typeChecker, Ast* ast) {
    uint8_t entryCount = 0;
    Ast* keys = astGetChild(ast, 0);
    Ast* values = astGetChild(ast, 1);
    while (entryCount < keys->children->count) {
        typeCheckChild(typeChecker, keys, entryCount);
        typeCheckChild(typeChecker, values, entryCount);
        entryCount++;
    }
    defineAstType(typeChecker, ast, "clox.std.collection.Dictionary", NULL);
}

static void typeCheckFunction(TypeChecker* typeChecker, Ast* ast) {
    ObjString* name = copyString(typeChecker->vm, ast->token.start, ast->token.length);
    TypeInfo* calleeType = typeTableGet(typeChecker->vm->typetab, name);
    function(typeChecker, ast, calleeType == NULL ? NULL : AS_CALLABLE_TYPE(calleeType), ast->modifier.isAsync, ast->modifier.isClass);
}

static void typeCheckGrouping(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    inferAstTypeFromChild(ast, 0, NULL);
}

static void typeCheckInterpolation(TypeChecker* typeChecker, Ast* ast) {
    Ast* exprs = astGetChild(ast, 0);
    int count = 0;

    while (count < exprs->children->count) {
        bool concatenate = false;
        bool isString = false;
        Ast* expr = astGetChild(exprs, count);

        if (expr->kind == AST_EXPR_LITERAL && expr->token.type == TOKEN_STRING) {
            concatenate = true;
            isString = true;
            count++;
            if (count >= exprs->children->count) break;
        }

        typeCheckChild(typeChecker, exprs, count);
        count++;
    }
    defineAstType(typeChecker, ast, "String", NULL);
}

static void typeCheckInvoke(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    inferAstTypeFromInvoke(typeChecker, ast);
}

static void typeCheckNil(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    inferAstTypeFromChild(ast, 0, NULL);
}

static void typeCheckOr(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    defineAstType(typeChecker, ast, "Bool", NULL);
}

static void typeCheckParam(TypeChecker* typeChecker, Ast* ast) {
    ObjString* name = copyString(typeChecker->vm, ast->token.start, ast->token.length);
    SymbolItem* item = symbolTableLookup(ast->symtab, name);
    item->type = ast->type;
}

static void typeCheckPropertyGet(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
}

static void typeCheckPropertySet(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    defineAstType(typeChecker, ast, "Nil", NULL);
}

static void typeCheckSubscriptGet(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    inferAstTypeFromSubscriptGet(typeChecker, ast);
}

static void typeCheckSubscriptSet(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    typeCheckChild(typeChecker, ast, 2);
    inferAstTypeFromSubscriptSet(typeChecker, ast);
}

static void typeCheckSuperGet(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    Ast* receiver = astGetChild(ast, 0);
    ObjString* property = copyString(typeChecker->vm, ast->token.start, ast->token.length);
    if (receiver->type == NULL) return;

    TypeInfo* superType = AS_BEHAVIOR_TYPE(receiver->type)->superclassType;
    TypeInfo* methodType = typeTableMethodLookup(superType, property);
    if (methodType != NULL) defineAstType(typeChecker, ast, "BoundMethod", NULL);
}

static void typeCheckSuperInvoke(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    inferAstTypeFromSuperInvoke(typeChecker, ast);
}

static void typeCheckThis(TypeChecker* typeChecker, Ast* ast) {
    if (typeChecker->currentClass->type) {
        if (typeChecker->currentFunction->isClass) {
            ObjString* className = getClassFullName(typeChecker->vm, createSymbol(typeChecker, typeChecker->currentClass->name), typeChecker->currentNamespace);
            ObjString* metaclassName = getMetaclassNameFromClass(typeChecker->vm, className);
            ast->type = typeTableGet(typeChecker->vm->typetab, metaclassName);
        }
        else {
            ast->type = &typeChecker->currentClass->type->baseType;
        }
    }
}

static void typeCheckTrait(TypeChecker* typeChecker, Ast* ast) {
    behavior(typeChecker, BEHAVIOR_TRAIT, ast);
}

static void typeCheckUnary(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    inferAstTypeFromUnary(typeChecker, ast, NULL);
}

static void typeCheckVariable(TypeChecker* typeChecker, Ast* ast) {
    SymbolItem* item = symbolTableLookup(ast->symtab, createSymbol(typeChecker, ast->token));
    ast->type = item->type;
}

static void typeCheckYield(TypeChecker* typeChecker, Ast* ast) {
    yield(typeChecker, ast);
    defineAstType(typeChecker, ast, "Generator", NULL);
}

static void typeCheckExpression(TypeChecker* typeChecker, Ast* ast) {
    switch (ast->kind) {
        case AST_EXPR_AND:
            typeCheckAnd(typeChecker, ast);
            break;
        case AST_EXPR_ARRAY:
            typeCheckArray(typeChecker, ast);
            break;
        case AST_EXPR_ASSIGN:
            typeCheckAssign(typeChecker, ast);
            break;
        case AST_EXPR_AWAIT:
            typeCheckAwait(typeChecker, ast);
            break;
        case AST_EXPR_BINARY:
            typeCheckBinary(typeChecker, ast);
            break;
        case AST_EXPR_CALL:
            typeCheckCall(typeChecker, ast);
            break;
        case AST_EXPR_CLASS:
            typeCheckClass(typeChecker, ast);
            break;
        case AST_EXPR_DICTIONARY:
            typeCheckDictionary(typeChecker, ast);
            break;
        case AST_EXPR_FUNCTION:
            typeCheckFunction(typeChecker, ast);
            break;
        case AST_EXPR_GROUPING:
            typeCheckGrouping(typeChecker, ast);
            break;
        case AST_EXPR_INTERPOLATION:
            typeCheckInterpolation(typeChecker, ast);
            break;
        case AST_EXPR_INVOKE:
            typeCheckInvoke(typeChecker, ast);
            break;
        case AST_EXPR_NIL:
            typeCheckNil(typeChecker, ast);
            break;
        case AST_EXPR_OR:
            typeCheckOr(typeChecker, ast);
            break;
        case AST_EXPR_PARAM:
            typeCheckParam(typeChecker, ast);
            break;
        case AST_EXPR_PROPERTY_GET:
            typeCheckPropertyGet(typeChecker, ast);
            break;
        case AST_EXPR_PROPERTY_SET:
            typeCheckPropertySet(typeChecker, ast);
            break;
        case AST_EXPR_SUBSCRIPT_GET:
            typeCheckSubscriptGet(typeChecker, ast);
            break;
        case AST_EXPR_SUBSCRIPT_SET:
            typeCheckSubscriptSet(typeChecker, ast);
            break;
        case AST_EXPR_SUPER_GET:
            typeCheckSuperGet(typeChecker, ast);
            break;
        case AST_EXPR_SUPER_INVOKE:
            typeCheckSuperInvoke(typeChecker, ast);
            break;
        case AST_EXPR_THIS:
            typeCheckThis(typeChecker, ast);
            break;
        case AST_EXPR_TRAIT:
            typeCheckTrait(typeChecker, ast);
            break;
        case AST_EXPR_UNARY:
            typeCheckUnary(typeChecker, ast);
            break;
        case AST_EXPR_VARIABLE:
            typeCheckVariable(typeChecker, ast);
            break;
        case AST_EXPR_YIELD:
            typeCheckYield(typeChecker, ast);
            break;
        default:
            break;
    }
}

static void typeCheckAwaitStatement(TypeChecker* typeChecker, Ast* ast) {
    await(typeChecker, ast);
}

static void typeCheckBlockStatement(TypeChecker* typeChecker, Ast* ast) {
    block(typeChecker, ast);
}

static void typeCheckCaseStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
}

static void typeCheckCatchStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    Ast* exceptionVar = astGetChild(ast, 0);
    ObjString* exceptionClassName = copyString(typeChecker->vm, ast->token.start, ast->token.length);
    exceptionVar->type = getClassType(typeChecker, exceptionClassName, NULL);

    Ast* blk = astGetChild(ast, 1);
    ObjString* exceptionVarName = copyString(typeChecker->vm, exceptionVar->token.start, exceptionVar->token.length);
    SymbolItem* exceptionItem = symbolTableLookup(blk->symtab, exceptionVarName);
    exceptionItem->type = exceptionVar->type;
    typeCheckChild(typeChecker, ast, 1);
}

static void typeCheckDefaultStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
}

static void typeCheckExpressionStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
}

static void typeCheckFinallyStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
}

static void typeCheckForStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    typeCheckChild(typeChecker, ast, 2);
}

static void typeCheckIfStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    if (astNumChild(ast) > 2) {
        typeCheckChild(typeChecker, ast, 2);
    }
}

static void typeCheckRequireStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    Ast* child = astGetChild(ast, 0);
    if (!isSubtypeOfType(child->type, typeChecker->stringType)) {
        typeError(typeChecker, "require statement expects expression to be an instance of String but gets %s.", child->type->shortName->chars);
    }
}

static void typeCheckReturnStatement(TypeChecker* typeChecker, Ast* ast) {
    TypeInfo* actualType = NULL;
    TypeInfo* expectedType = (typeChecker->currentFunction->type != NULL) ? typeChecker->currentFunction->type->returnType : NULL;
    if (expectedType == NULL) return;

    if (astHasChild(ast)) {
        typeCheckChild(typeChecker, ast, 0);
        actualType = astGetChild(ast, 0)->type;
    }
    else actualType = typeChecker->nilType;

    if (!isSubtypeOfType(actualType, expectedType)) {
        typeError(typeChecker, "Function expects return value to be an instance of %s but gets %s.", 
            expectedType->shortName->chars, actualType->shortName->chars);
    }
}

static void typeCheckSwitchStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    Ast* caseList = astGetChild(ast, 1);

    for (int i = 0; i < caseList->children->count; i++) {
        typeCheckChild(typeChecker, caseList, i);
    }

    if (astNumChild(caseList) > 2) {
        typeCheckChild(typeChecker, ast, 2);
    }
}

static void typeCheckThrowStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    Ast* child = astGetChild(ast, 0);
    if (!hasAstType(typeChecker, child, "clox.std.lang.Exception")) {
        typeError(typeChecker, "throw statement expects expression to be an instance of Exception but gets %s.", child->type->shortName->chars);
    }
}

static void typeCheckTryStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    if (astNumChild(ast) > 2) {
        typeCheckChild(typeChecker, ast, 2);
    }
}

static void typeCheckUsingStatement(TypeChecker* typeChecker, Ast* ast) {
    Ast* _namespace = astGetChild(ast, 0);
    int namespaceDepth = astNumChild(_namespace);
    ObjString* fullName = astCreateQualifiedName(typeChecker->vm, ast);
    TypeInfo* namespaceType = getNativeType(typeChecker->vm, "Namespace");

    for (int i = 0; i < namespaceDepth - 1; i++) {
        Ast* subNamespace = astGetChild(_namespace, i);
        subNamespace->type = namespaceType;
    }

    TypeInfo* classType = typeTableGet(typeChecker->vm->typetab, fullName);
    if (classType == NULL) return;
    Ast* child = (astNumChild(ast) > 1) ? astGetChild(ast, 1) : astLastChild(_namespace);
    ObjString* shortName = copyString(typeChecker->vm, child->token.start, child->token.length);

    if (classType->category == TYPE_CATEGORY_TRAIT) child->type = getNativeType(typeChecker->vm, "Trait");
    else child->type = typeTableGet(typeChecker->vm->typetab, getMetaclassNameFromClass(typeChecker->vm, fullName));
    SymbolItem* item = symbolTableLookup(child->symtab, shortName);
    if (item->type == NULL) item->type = child->type;
}

static void typeCheckWhileStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
}

static void typeCheckYieldStatement(TypeChecker* typeChecker, Ast* ast) {
    yield(typeChecker, ast);
}

static void typeCheckStatement(TypeChecker* typeChecker, Ast* ast) {
    switch (ast->kind) {
        case AST_STMT_AWAIT:
            typeCheckAwaitStatement(typeChecker, ast);
            break;
        case AST_STMT_BLOCK:
            typeCheckBlockStatement(typeChecker, ast);
            break;
        case AST_STMT_CASE:
            typeCheckCaseStatement(typeChecker, ast);
            break;
        case AST_STMT_CATCH:
            typeCheckCatchStatement(typeChecker, ast);
            break;
        case AST_STMT_DEFAULT:
            typeCheckDefaultStatement(typeChecker, ast);
            break;
        case AST_STMT_EXPRESSION:
            typeCheckExpressionStatement(typeChecker, ast);
            break;
        case AST_STMT_FINALLY:
            typeCheckFinallyStatement(typeChecker, ast);
            break;
        case AST_STMT_FOR:
            typeCheckForStatement(typeChecker, ast);
            break;
        case AST_STMT_IF:
            typeCheckIfStatement(typeChecker, ast);
            break;
        case AST_STMT_REQUIRE:
            typeCheckRequireStatement(typeChecker, ast);
            break;
        case AST_STMT_RETURN:
            typeCheckReturnStatement(typeChecker, ast);
            break;
        case AST_STMT_SWITCH:
            typeCheckSwitchStatement(typeChecker, ast);
            break;
        case AST_STMT_THROW:
            typeCheckThrowStatement(typeChecker, ast);
            break;
        case AST_STMT_TRY:
            typeCheckTryStatement(typeChecker, ast);
            break;
        case AST_STMT_USING:
            typeCheckUsingStatement(typeChecker, ast);
            break;
        case AST_STMT_WHILE:
            typeCheckWhileStatement(typeChecker, ast);
            break;
        case AST_STMT_YIELD:
            typeCheckYieldStatement(typeChecker, ast);
            break;
        default:
            break;
    }
}

static void typeCheckClassDeclaration(TypeChecker* typeChecker, Ast* ast) {
    ObjString* className = createSymbol(typeChecker, ast->token);
    ObjString* metaclassName = getMetaclassNameFromClass(typeChecker->vm, className);
    TypeInfo* metaclassType = getClassType(typeChecker, metaclassName, ast->symtab);

    ast->type = metaclassType;
    SymbolItem* item = symbolTableGet(ast->symtab, className);
    if (item != NULL) item->type = ast->type;

    Ast* _class = astGetChild(ast, 0);
    _class->type = ast->type;
    typeCheckChild(typeChecker, ast, 0);
}

static void typeCheckFunDeclaration(TypeChecker* typeChecker, Ast* ast) {
    SymbolItem* item = symbolTableGet(ast->symtab, createSymbol(typeChecker, ast->token));
    defineAstType(typeChecker, ast, "Function", item);
    Ast* function = astGetChild(ast, 0);
    function->type = ast->type;
    typeCheckChild(typeChecker, ast, 0);
}

static void typeCheckMethodDeclaration(TypeChecker* typeChecker, Ast* ast) {
    SymbolItem* item = symbolTableLookup(ast->symtab, createSymbol(typeChecker, ast->token));
    ObjString* name = copyString(typeChecker->vm, item->token.start, item->token.length);
    defineAstType(typeChecker, ast, "Method", item);

    if (!typeChecker->currentClass->isAnonymous) {
        BehaviorTypeInfo* classType = NULL;
        if (ast->modifier.isClass) {
            ObjString* className = getClassFullName(typeChecker->vm, createSymbol(typeChecker, typeChecker->currentClass->name), typeChecker->currentNamespace);
            ObjString* metaclassName = getMetaclassNameFromClass(typeChecker->vm, className);
            classType = AS_BEHAVIOR_TYPE(typeTableGet(typeChecker->vm->typetab, metaclassName));
        }
        else classType = typeChecker->currentClass->type;

        CallableTypeInfo* methodType = AS_CALLABLE_TYPE(typeTableGet(classType->methods, name));
        function(typeChecker, ast, methodType, ast->modifier.isAsync, ast->modifier.isClass);
    }
}

static void typeCheckNamespaceDeclaration(TypeChecker* typeChecker, Ast* ast) {
    Ast* identifiers = astGetChild(ast, 0);
    Ast* identifier = astGetChild(identifiers, 0);
    SymbolItem* item = symbolTableGet(ast->symtab, createSymbol(typeChecker, identifier->token));
    defineAstType(typeChecker, ast, "Namespace", item);
    typeChecker->currentNamespace = astCreateQualifiedName(typeChecker->vm, ast);
}

static void typeCheckTraitDeclaration(TypeChecker* typeChecker, Ast* ast) {
    SymbolItem* item = symbolTableGet(ast->symtab, createSymbol(typeChecker, ast->token));
    defineAstType(typeChecker, ast, "Trait", item);
    Ast* trait = astGetChild(ast, 0);
    trait->type = ast->type;
    typeCheckChild(typeChecker, ast, 0);
}

static void typeCheckVarDeclaration(TypeChecker* typeChecker, Ast* ast) {
    SymbolItem* item = symbolTableGet(ast->symtab, createSymbol(typeChecker, ast->token));
    if (astHasChild(ast)) {
        typeCheckChild(typeChecker, ast, 0);
        if (!ast->modifier.isMutable) inferAstTypeFromChild(ast, 0, item);
    }
}

static void typeCheckDeclaration(TypeChecker* typeChecker, Ast* ast) {
    switch (ast->kind) {
        case AST_DECL_CLASS:
            typeCheckClassDeclaration(typeChecker, ast);
            break;
        case AST_DECL_FUN:
            typeCheckFunDeclaration(typeChecker, ast);
            break;
        case AST_DECL_METHOD:
            typeCheckMethodDeclaration(typeChecker, ast);
            break;
        case AST_DECL_NAMESPACE:
            typeCheckNamespaceDeclaration(typeChecker, ast);
            break;
        case AST_DECL_TRAIT:
            typeCheckTraitDeclaration(typeChecker, ast);
            break;
        case AST_DECL_VAR:
            typeCheckVarDeclaration(typeChecker, ast);
            break;
        default:
            break;
        }
}

void typeCheckAst(TypeChecker* typeChecker, Ast* ast) {
    for (int i = 0; i < ast->children->count; i++) {
        typeCheckChild(typeChecker, ast, i);
    }
}

void typeCheckChild(TypeChecker* typeChecker, Ast* ast, int index) {
    Ast* child = astGetChild(ast, index);
    typeChecker->currentToken = child->token;

    switch (child->category) {
        case AST_CATEGORY_SCRIPT:
        case AST_CATEGORY_OTHER:
            typeCheckAst(typeChecker, child);
            break;
        case AST_CATEGORY_EXPR:
            typeCheckExpression(typeChecker, child);
            break;
        case AST_CATEGORY_STMT:
            typeCheckStatement(typeChecker, child);
            break;
        case AST_CATEGORY_DECL:
            typeCheckDeclaration(typeChecker, child);
            break;
        default:
            typeError(typeChecker, "Invalid AST category.");
    }
}

void typeCheck(TypeChecker* typeChecker, Ast* ast) {
    FunctionTypeChecker functionTypeChecker;
    initFunctionTypeChecker(typeChecker, &functionTypeChecker, syntheticToken("script"), NULL, ast->modifier.isAsync, false);
    typeCheckAst(typeChecker, ast);

    endFunctionTypeChecker(typeChecker);
    if (typeChecker->debugTypetab) {
        typeTableOutput(typeChecker->vm->typetab);
    }
}