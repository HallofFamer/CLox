# Change Log

### Lox 2.0.0(current version)
- Multi-pass compiler with abstract syntax tree, semantic analyzer(resolver), symbol table, type checker, and generation of bytecode by walking AST. 
- Optional static typing support for function/method parameters and return values, types only exist at compile time and are erased at runtime. 
- Semicolon inference as well as basic type inference for immutable local/global variables. 
- Replace the old mark and sweep GC with a generational GC which has multiple regions for objects of various generations.  

### Lox 1.9.0(last version)
- Generator functions/methods which can yield control back to the caller and resume at a later point of execution.
- Add class `Promise` to the standard library(`clox.std.util`), which uses libuv to handle async tasks that completes in future. 
- Introduction of `async` and `await` keywords, which allows C#/JS style of concurrency.
- Refactoring package `clox.std.io` and `clox.std.net` to use async non-blocking calls.

### Lox 1.8.0
- Operator overloading to allow operators to be treated as method calls, thus can be used by user defined classes.
- Improved string concatenation, addition of string interpolation and UTF-8 strings.
- Interceptor methods which are invoked automatically when getting/setting properties, invoking methods or throwing exceptions.
- Object ID and generic object map which enable inheritance for special build-in classes such as `String` and `Array`.

### Lox 1.7.0
- Raise exception with `throw` keyword, and exception handling with try/catch/finally statement.
- Improved CLox standard library with addition of class `Exception` and various exception subclasses.
- Addition of nil handling operators: Optional chaining operator(?.), Nil coalescing operator(??), and Elvis operator(?:). 
- Inline caching for VM optimization, as well as implementation of Shape(Hidden Class) for better instance variable representation.

### Lox 1.6.0
- Namespace as CLox's module system, allowing importing namespace and aliasing of imported classes, traits and functions.
- Refactor the existing standard library with namespaces(in `clox.std` parent package), add new package `clox.std.net`.
- Fix reentrancy problem with CLox, calling Lox closures from within C API becomes possible.
- Cross-platform build with Cmake, as well as package manager with vcpkg(windows only).

### Lox 1.5.0
- Refined object model which is similar to Smalltalk's metaclass system.
- Class methods in class declaration, and `trait` keyword for trait declaration.
- Allow loading lox source files in lox script and another lox source file with `require` keyword. 
- Anonymous classes/traits similar to anonymous functions/lambda.

### Lox 1.4.0
- Immutable variable declaration with `val` keyword.
- Function/Method parameters become immutable by default, but may be mutable with `var` keyword.
- Built-in and user defined classes/functions become be immutable, and cannot be accidentally overwritten. 
- New class `Range` in package `collection`, as well as range operator(`..`) for range literals. 

### Lox v1.3.0
- Array/Dictionary Literals and square bracket(subscript) notation for array/dictionary access.
- Variadic Functions, Anonymous Functions(local return) and Lambda Expressions(nonlocal return).
- Replace C style for loop by Kotlin style for-in loop for collection types.
- Clox Standard Library improvement: New package `collection` and `io`.

### Lox v1.2.0
- Improved object model - Everything is an object, and every object has a class, including `nil`, `true`, `false`, `number`, `string`, `function`, `class` etc.
- CLox Standard Library for package `lang` and `util`, which contains classes such as `Boolean`, `Number`, `String`, `Array`, `Dictionary`, `DateTime`, etc.
- Allow customized runtime configurations for CLox at startup with clox.ini
- Split the `Number` class into `Int` and `Float` classes, which will distinguish between integers and floating numbers.

### CLox v1.1.0
- VM is no longer a global variable, allowing CLox VM to be embedded in other host applications.
- Full fledged Framework for writing Native functions, methods and classes.
- Root class `Object` which serves as superclass of every class.
- Remove print statement and replace it by `print` and `println` native functions.