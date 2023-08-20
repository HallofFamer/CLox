# CLox
Another implementation of ByteCode Interpreter for Lox in C, with tons of new features.


## Introduction
CLox is an implementation of the programming language Lox in C. Currently it uses naive single-pass compiler, and only runtime optimization is performed. In future it is planned to have a multi-pass compiler with AST and compiler optimization. The initial version of CLox has only features already present in the original Lox reference implementation, but subsequent versions will continue to add new features to make it a powerful language. This is an experiment on the design and implementation of language features, before I start to implement the backend of my own programming language Mysidia. Stay tuned. 

The original version of Lox programming language can be found at Bob Nystrom's repository:
https://github.com/munificent/craftinginterpreters


## Features
- Scanner, Parser and Single-Pass Compiler
- Stacked based bytecote VM with the basic Op Code support
- Unary and Binary Expression/Operators
- If and Else condition Statement
- Switch Condition Statement
- While Loop, For Loop and Break Statement
- Functions and Closures with automatic upvalue capture
- Classes, Objects, Methods and `this` keyword
- Single Inheritance and `super` keyword
- Embeddable VM for CLox in other host applications(since version 1.1)
- `Object` root class for every class in Lox(since version 1.1)
- Framework for creating native functions, methods and classes(since version 1.1)
- Print statement removed, use native function `print` and `println` instead(since version 1.1)
- Improved Object Model - Everything is an object, and every object has a class(since version 1.2)
- CLox Standard Library for packages `lang` and `util`(since version 1.2)
- Customized Runtime configuration for CLox using clox.ini(since version 1.2)
- Separated integer values and floating point values(since version 1.2)
- Array/Dictionary Literals and square bracket notation for array/dictionary access(since version 1.3)
- Variadic Functions, Anonymous Functions and Lambda Expressions(since version 1.3)
- C style For Loop replaced with Kotlin style for-in Loop(since version 1.3)
- Clox Standard Library for new packages `collection` and `io`(since version 1.3)
- Immutable variable declaration with `val` keyword for global and local variables(since version 1.4)
- Function/Method parameters become immutable by default, but may be mutable with `var` keyword(since version 1.4)
- Built-in and user defined classes/functions become be immutable, and cannot be accidentally overwritten(since version 1.4)
- New class `Range` in package `util`, as well as range operator(`..`) for range literals(since version 1.4) 
- Refined object model which is similar to Smalltalk's metaclass system(since version 1.5)
- Class methods in class declaration, and `trait` keyword for trait declaration(since version 1.5)
- Allow loading lox source files in lox script and another lox source file with `require` keyword(since version 1.5)
- Anonymous classes/traits similar to anonymous functions/lambda(since version 1.5)
- Namespace as CLox's module system, allowing importing namespace and aliasing of imported classes, traits and functions(since version 1.6)
- Refactor the existing standard library in `clox.std` namespace(since version 1.6)
- Fix reentrancy problem with CLox, calling Lox closures in C API becomes possible(since version 1.6)
- Cross-platform build with Cmake and package manager with vcpkg.


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

### CLox 1.5.0(current version)
- Refined object model which is similar to Smalltalk's metaclass system.
- Class methods in class declaration, and `trait` keyword for trait declaration.
- Allow loading lox source files in lox script and another lox source file with `require` keyword. 
- Anonymous classes/traits similar to anonymous functions/lambda.

### CLox 1.6.0(upcoming version)
- Namespace as CLox's module system, allowing importing namespace and aliasing of imported classes, traits and functions.
- Refactor the existing standard library with namespaces(in `clox.std` package), add new package `clox.std.network`.
- Fix reentrancy problem with CLox, calling Lox closures in C API becomes possible.
- Cross-platform build with Cmake and package manager with vcpkg.

### CLox 1.7.0
- Raise exception with `throw` keyword, and exception handling with try/catch/finally statement.
- Improved CLox standard library with addition of class `Exception` and various exception subclasses.
- Null safe operator (?.) which short-circuit if nil is found in a long expression.
- VM optimization for instance variable representations, and inline caching.

### CLox 1.8.0
- Operator overloading to allow operators to be treated as method calls, thus can be used by user defined classes.
- Improved string concatenation, addition of string interpolation and UTF-8 strings.
- Method interception when an undefined method call is invoked on an object/class, similar to Smalltalk's doesNotUnderstand: message.
- Object ID and generic object table which enable inheritance for special build-in classes such as `String` and `Array`.


## Build and Run Clox

#### Windows(with vcpkg)
```
git clone -b master https://github.com/HallofFamer/CLox.git
cd CLox
mkdir build
cmake -DCMAKE_TOOLCHAIN_FILE:STRING="[$VCPKG_PATH]/scripts/buildsystems/vcpkg.cmake" -B ./build
cmake --build ./build --config Release
cd x64/Release
./clox
```

#### Linux
Coming Soon!


## FAQ

#### What is the purpose of this project?
CLox is an implementation of Lox language with bytecode VM instead of treewalk interpreter. It is the last experiment on feature implementations, before I will begin writing my own programming language `Mysidia` in C.

#### What will happen to KtLox?
Nothing, KtLox development is on hold until I can figure out a way to generate JVM bytecode instead of running the interpreter by walking down the AST. Treewalk interpreters are way too slow to be practical without JIT. 