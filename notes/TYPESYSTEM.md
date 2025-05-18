# Type System

Lox2 comes with an optional type system which also performs simple type inference for immutable variables. The type system initially has only simple types, and allows type annotations for function/method param and return types. The future versions will enhance the type system further. 

## Nominal vs Structural Types
Lox2 has a nominal type system which means that types are defined by their names, similar to Java and C#. This is in contrast with structural type system prevalent in Typescript and GoLang. Partial support for structural types may be added in future, but it is among the lowest priorities.

## Subtyping
As an object oriented language, Lox2 naturally supports subtyping. For class and trait types, the subtyping relation is determined by inheritance relation. If class B is a subclass of class A, then class B is a subtype of class A. This allows instance of class B to be used where an instance of class A is expected. This behavior is consistent with mainstream OO languages. 
One exception to the rule is the Nil type, which is currently a subtype of all types, thus a bottom type. In future this behavior will change and Nil will no longer be the bottom type. This will ensure better nil safety. 

## Type Annotation Syntax
Lox2 adopts the same type annotation syntax as C++, Java, C# and Dart, which looks like `TypeName VarName`. This design decision is largely personal, as I have a strong preference for this style of type annotation syntax coming from years of programming experience using C, Java and C#. 
It does complicate the parser, which is a challenge I gladly accept to enhance the parser with infinite look-ahead and backtrack in Lox 2.1. The choice also frees up colons that allows labeled methods to be implemented if desired one day. 

## Function/Method Param and Return Types
The initial release of Lox2 allows programmers to optionally specify type annotations for the function/method param and return types. For return types, the type annotation precedes the `fun` keyword or method name, as the below example demonstrates:

```
Int fun gcd(Int first, Int second) {}

class Calculator {
    Number calculateHypot(Number a, Number b) {}
}

gcd(true, "Hello")
// fail to compile with type error.
```

In this case, a Lox2 script will fail to compile if calling the function/method with wrong param types, or the following function/method returns a different type than expected. It allows some ill-typed programs to be caught earlier in the type checker, without actually running the code in interpreter. 

## Dynamic and Void Types
If type annotations are omitted for function/method signatures, they will automatically be assigned type dynamic, which means their values can change to any valid types in Lox2. The same is also true for nodes that cannot be type checked at this moment, though future work is planned to allow most Lox2 programs to be type checked. 
There is also a special `void` type which may be used as function/method return type only. Trying to return a value in a void function/method is a compile type error. Also for functions, the keyword `fun` may be omitted. 

## Local Variable Types
Lox2 opts to disallow explicit type annotations for local variables. This may be controversial, but largely thanks to the Java community’s reluctance to move on from explicitly types local variables to using `var` for type inference, even in the most obvious use cases. With Lox2, programmers do not have to decide to use explicit types or type inference for local variables, the language makes the choice for them. 
For immutable variables declared with `val`, the compiler uses simple type inference to deduce the type based on the right hand side of the initializer expression. For mutable variables declared with `var`, at this very moment everything defaults to dynamic. This may seem inconvenient, but it encourages developers to default to declaring immutable variables. Moreover, it will make writing a future JIT compiler meaningful and interesting. 
In distant future, Lox3 will change the meaning of `var` so it behaves similar to C# and Kotlin that the types are fixed and inferred by usage. A new keyword `dynamic` will be introduced and behave as how `var` does in Lox2. 

## Type Inference
At the very beginning Lox2 has simple type inference for immutable local variables, and this will be extended for field types and function/lambda types in Lox 2.1. 
Type inference for function types are explained in more details at the Function/Lambda Types section. 

## Field Types
The initial version of Lox2 does not have fields declared outside of initializer/method bodies. For this reason, instance and class fields cannot be typed. Lox 2.1 will introduce instance field declaration inside the class body, allowing instance and class fields to be typed. The below program demonstrates how this will work:
```
class Device {
    val String name
    val batteryLife = 5
    var String owner
    class val serialID = "123456"
}
```
Here the type annotations are used to declare types for instance and class fields. If type annotations are omitted, they default to dynamic. However, if initializer is specified for an immutable field, it can deduce the type from the initializer similar to how it works for local variables. In the above case, the field batteryLife is inferred to have type Int instead of Dynamic, and class field serialID has its type inferred to be String. 

## Metaclass Types
As classes themselves are objects, they can be assigned to variables, passed to functions/methods as arguments and returned as values. This implies need to allow type annotations for metaclass types, which will be added in Lox 2.1. The syntax is `(MetaclassType class)`, note the enclosing parenthesis is mandatory. For instance the below code allows returning class Date instead of instance of Date. 
```
(UserFactory class) fun getUserFactory() {}
```
Lox2 has an object model similar to Smalltalk, which specifies that metaclass hierarchy mirrors class hierarchy. In the above example, the function may return either class UserFactory, or a subclass of this factory.

## Function/Lambda Types
Lox 2.1 will also enable programmers to specify type annotations for anonymous functions and lambda expressions. The syntax is `ReturnType fun(ParamType1, ParamType2, …)` as inspired by Dart. See the examples below for more details, note the keyword `fun` is important and cannot be omitted. 
```
Int fun(Int, Int) 
void fun()
```
At use site, an anonymous function or lambda expressions will have their param/return type inferred unless it comes from typed function/method param, return type or field type. 

## Qualified Names as Types
Another new feature for type system coming to Lox 2.1 is that programmers may use qualified names in type annotations. This was not possible in the initial release of Lox2 due to the limitation of the parser, but will no longer be a hindrance in Lox2.1+. This allows the following program to parse and compile successfully:
```
clox.std.util.Promise fun fetchAsync(clox.std.net.URL url) {}
```
Oh the other hand, the resolver and type checker will be able to deduce types for AST with qualified names. The expression `clox.std.collection.LinkedList()` will have type LinkedList assigned to the node, instead of dynamic. 

## Generic Types
Lox 2.2 will introduce generic types which enables parametric polymorphism, and will be immensely useful for collection types, promises, etc. The syntax is similar to most mainstream languages, with type parameters enclosed inside a pair of angle brackets, ie. `GenericType<T1, T2, …>`. Again this adds complexity to the parser, but it is the most familiar syntax for most developers and Lox2’s lack of shift operators makes this complexity somewhat manageable. Generic types may be defined at both the class/trait level and function/method level. 
```
class Repository<TEntity> {} 
void registerDependency<TInterface,TImplentation>() {}
```
At use site, generic parameters need to be substituted by concrete types. Lox2’s generics are reified and it is possible to inspect the type parameter at runtime. 

## Variance
Lox 2.2 will refine the subtyping rules for function types to allow function return types to be covariant. This means `Int fun(String)` is a subtype of `Number fun(String)`. 
The same subtyping rules is allowed when a method in subclass overrides one from inherited superclass or implemented traits. The below code example is a valid Lox2 program:
```
class Animal {}
class Dog extends Animal {}

class AnimalShelter {
    Animal getByName(String name) {}
}

class DogShelter extends AnimalShelter {
    Dog getByName(String name) {}
}
// Will be valid in Lox 2.2+
```
The same subtyping rules will not work for param types, as type theory dictates that param types are contravariant. As contravariance is confusing and rarely useful in practice, Lox2 does not support contravariant param types. 

## Type Alias
Another planned feature for Lox 2.2 is type alias, which uses keyword `type` similar to typescript. Type alias will come in handy when declaring complex generic and function types. Once created they can be substituted where the actual types are used. 
```
type IDMap = Dictionary<String, ID<Int>>
ID<Int> fun getID(IDMap map, String key) {}
```
Type alias can also be generic, the below type alias creates a predicate with type parameter T:
```
type Predicate<T> = Bool fun(T)
```
Type alias are similar to classes and traits, that their declaration creates an entry in the shared/global values in the VM. This means they can be imported with the `using` statement to use in a different source file. 

## Bounded Generics
Lox 2.3 will add constraints to the basic generics supported in Lox 2.2, which will work in similar way as Java. The syntax will be `GenericType<T extends S>`, which restricts type parameter T to be a subtype of S. For instance, the below example creates a repository of type parameter that must be a subclass of Entity. 
```
class Repository<T extends Entity> {} 
```

## Union Types
Another key feature to be introduced in Lox 2.3 is union types, which works similar to Typescript. A union type consists of several types separated with pipe operator `|` , the syntax looks like `T1 | T2 | T3`. Below are common use cases for Union Types.
```
void setAge(Int | Nil age)
Bool fun isValidURL(String | URL url)
```
Lox2 performs compile time narrowing to factor out subtypes from the union type. For instance, the below two union types are equivalent:
```
String | Number | Int
String | Number
```
Union types are also very good candidates for creation of type alias, it may be desirable to create an Option type like this:
```
type Option<T> = T | Nil
```

## Nil Type
The literal value `nil` has type `Nil` which is also the name of its class. In Lox 2.2-, `Nil` is a bottom type which means all types are effectively nullable. However, this behavior will change in Lox 2.3, as types are no longer nullable by default. The below code will become invalid:
```
void setAge(Int age) {}
setAge(nil)
// In Lox 2.3 this is a compile time error
```
With the introduction of union types, it is possible to specify nullable types if needed. The above example can be corrected as:
```
void setAge(Int | Nil age) {}
setAge(nil)
// This will compile succeafully
```
This aligns with the nil safety standard as seen in Typescript and Kotlin. A dedicated syntax for Nullable types may be introduced later, ie. `T?`. But it is not a priority at the moment. 

## Further Enhancements
Lox 2.4 and beyond may continue to improve the type system, and this note will be updated to reflect this. With this being said, Lox2  is meant to be an education language for building industrial/production graded compilers/interpreters, but not for research in academia. 

For this reason, the type system is expected to be expressive and yet simple enough to understand. There is no plan to add complicated type level programming features such as Higher Kinded Types and Dependent Types to the language. 