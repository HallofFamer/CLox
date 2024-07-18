# CLox
Another implementation of ByteCode Interpreter for Lox in C, with tons of new features.


## Introduction
CLox is an implementation of the programming language Lox in C. Currently it uses naive single-pass compiler, and only runtime optimization is performed. In future it is planned to have a multi-pass compiler with AST and compiler optimization. The initial version of CLox has only features already present in the original Lox reference implementation, but subsequent versions will continue to add new features to make it a powerful language. This is an experiment on the design and implementation of language features, before I start to implement the backend of my own programming language Mysidia. Stay tuned. 

The original version of Lox programming language can be found at Bob Nystrom's repository:
https://github.com/munificent/craftinginterpreters


## Features

### Original Features
- Stacked based bytecode VM with the basic OPCode support.
- On-demand Scanner, Pratt Parser and Single-Pass Compiler.
- Uniform runtime representation for Lox Value. 
- Basic Unary and Binary Expression/Operators.
- Support for global and local variables.
- If..Else and Switch condition Statement.
- While Loop, Break and Continue Statement.
- Functions and Closures with automatic upvalue capture.
- Mark and sweep Garbage Collector.
- Classes, Objects, Methods and `this` keyword.
- Single Inheritance and `super` keyword.
- Performance improvement with Faster hash table probing and Nan-Boxing.

### New Features
- Framework for creating native functions, methods and classes.
- Array/Dictionary Literals and square bracket notation for array/dictionary access.
- New Operators: Modulo(`%`), Range(`..`) and Nil Handling(`?.`, `??`, `?:`).
- Operator overloading to allow operators to be treated as method calls, thus can be used by user defined classes.
- Variadic Functions, Anonymous Functions(local return) and Lambda Expressions(nonlocal return).
- `Object` root class for every class in Lox, everything is an object, and every object has a class.
- Object ID and generic object map which enable inheritance for special build-in classes such as `String` and `Array`.
- Class methods in class declaration using `class` keyword, and trait declaration using `trait` keyword.
- Anonymous classes/traits similar to anonymous functions/lambda.
- Namespace as CLox's module system, allowing importing namespace and aliasing of imported classes, traits and functions.
- CLox Standard Library for packages `lang`, `util`, `collection`, `io` and `net` in bullt-in namespace `clox.std`.
- Raise exception with `throw` keyword, and exception handling with try/catch/finally statement.
- Interceptor methods which are invoked automatically when getting/setting properties, invoking methods or throwing exceptions.
- Generator functions/methods which can yield control back to the caller and resume at a later point of execution.
- Promise API with event loop provided by libuv library for non-blocking IO operations.
- Introduction of `async` and `await` keywords, which allows C#/JS style of concurrency.
- Customized Runtime configuration for CLox using clox.ini.
- Allow loading lox source files in lox script and another lox source file with `require` keyword.
- Cross-platform build with Cmake and package manager with vcpkg.

### Enhanced or Removed Features
- VM is no longer a global variable, allowing CLox VM to be embedded in other host applications.
- Parser is extended with look-ahead capability, with field next storing the next token. 
- Print statement removed, use native function `print` and `println` instead.
- Initializer method is renamed from `init` to `__init__` as an interceptor method.
- Separated integer values(`Int`) and floating point(`Float`) values from `Number`.
- Improved string concatenation, addition of string interpolation and UTF-8 strings.
- C style For Loop replaced with Python/Kotlin style for-in Loop.
- Global variables are scoped within the file it is declared, effectively becoming module variable.
- Function/Method parameters become immutable by default, but may be mutable with `var` keyword.
- Built-in and user defined classes/functions become be immutable, and cannot be accidentally overwritten.
- Fix reentrancy problem with CLox, calling Lox closures in C API becomes possible.
- Most runtime errors in VM interpreter loop replaced with Exceptions that can be caught at runtime.
- Inline caching for VM optimization, as well as implementation of Shape(Hidden Class) for better instance variable representation.


## Roadmap

### CLox v1.1.0
- VM is no longer a global variable, allowing CLox VM to be embedded in other host applications.
- Full fledged Framework for writing Native functions, methods and classes.
- Root class `Object` which serves as superclass of every class.
- Remove print statement and replace it by `print` and `println` native functions.

### CLox v1.2.0
- Improved object model - Everything is an object, and every object has a class, including `nil`, `true`, `false`, `number`, `string`, `function`, `class` etc.
- CLox Standard Library for package `lang` and `util`, which contains classes such as `Boolean`, `Number`, `String`, `Array`, `Dictionary`, `DateTime`, etc.
- Allow customized runtime configurations for CLox at startup with clox.ini
- Split the `Number` class into `Int` and `Float` classes, which will distinguish between integers and floating numbers.

### CLox v1.3.0
- Array/Dictionary Literals and square bracket(subscript) notation for array/dictionary access.
- Variadic Functions, Anonymous Functions(local return) and Lambda Expressions(nonlocal return).
- Replace C style for loop by Kotlin style for-in loop for collection types.
- Clox Standard Library improvement: New package `collection` and `io`.

### CLox 1.4.0
- Immutable variable declaration with `val` keyword.
- Function/Method parameters become immutable by default, but may be mutable with `var` keyword.
- Built-in and user defined classes/functions become be immutable, and cannot be accidentally overwritten. 
- New class `Range` in package `collection`, as well as range operator(`..`) for range literals. 

### CLox 1.5.0
- Refined object model which is similar to Smalltalk's metaclass system.
- Class methods in class declaration, and `trait` keyword for trait declaration.
- Allow loading lox source files in lox script and another lox source file with `require` keyword. 
- Anonymous classes/traits similar to anonymous functions/lambda.

### CLox 1.6.0
- Namespace as CLox's module system, allowing importing namespace and aliasing of imported classes, traits and functions.
- Refactor the existing standard library with namespaces(in `clox.std` parent package), add new package `clox.std.net`.
- Fix reentrancy problem with CLox, calling Lox closures from within C API becomes possible.
- Cross-platform build with Cmake, as well as package manager with vcpkg(windows only).

### CLox 1.7.0
- Raise exception with `throw` keyword, and exception handling with try/catch/finally statement.
- Improved CLox standard library with addition of class `Exception` and various exception subclasses.
- Addition of nil handling operators: Optional chaining operator(?.), Nil coalescing operator(??), and Elvis operator(?:). 
- Inline caching for VM optimization, as well as implementation of Shape(Hidden Class) for better instance variable representation.

### CLox 1.8.0(current version)
- Operator overloading to allow operators to be treated as method calls, thus can be used by user defined classes.
- Improved string concatenation, addition of string interpolation and UTF-8 strings.
- Interceptor methods which are invoked automatically when getting/setting properties, invoking methods or throwing exceptions.
- Object ID and generic object map which enable inheritance for special build-in classes such as `String` and `Array`.

### CLox 1.9.0(next version)
- Generator functions/methods which can yield control back to the caller and resume at a later point of execution.
- Add class `Promise` to the standard library(`clox.std.util`), which uses libuv to handle async tasks that completes in future. 
- Introduction of `async` and `await` keywords, which allows C#/JS style of concurrency.
- Refactoring package `clox.std.io` and `clox.std.net` to use async non-blocking calls.

### CLox 2.0.0
- Multi-pass compiler with abstract syntax tree, type checker, and generation of IR. 
- Optional static typing support for function/method parameters and return values, types only exist at compile time(erased at runtime). 
- Semicolon inference as well as basic type inference for immutable local/global variables. 
- Replace the naive mark and sweep GC with a generational GC which has multiple regions for objects of various 'ages'.  


## Build and Run Clox

#### Windows(with git, cmake and vcpkg, need to replace [$VCPKG_PATH] with installation path of vcpkg)
```
git clone -b master https://github.com/HallofFamer/CLox.git
cd CLox
cmake -DCMAKE_TOOLCHAIN_FILE:STRING="[$VCPKG_PATH]/scripts/buildsystems/vcpkg.cmake" -S . -B ./build
cmake --build ./build --config Release
./x64/Release/CLox
```

#### Linux(with git, cmake, curl and libuv, need to install one of the libcurl4-dev and libuv1.dev packages)
```
git clone -b master https://github.com/HallofFamer/CLox.git
cd CLox
mkdir build
cmake -S . -B ./build
cmake --build ./build --config Release
./build/CLox
```

#### Docker(linux, need to replace [$LinuxDockerfile] by actual docker file name, ie. UbuntuDockerfile)
```
git clone -b master https://github.com/HallofFamer/CLox.git
cd CLox
docker build -t clox:linux -f Docker/[$LinuxDockerfile] .
docker run -w /CLox-1.8.0/CLox -i -t clox:linux
```


## FAQ

#### What is the purpose of this project?
CLox is an implementation of Lox language with bytecode VM instead of treewalk interpreter. It is the last experiment on feature implementations, before I will begin writing my own programming language `Mysidia` in C.

#### What will happen to KtLox?
Nothing, KtLox development is on hold until I can figure out a way to generate JVM bytecode instead of running the interpreter by walking down the AST. Treewalk interpreters are way too slow to be practical without JIT. 