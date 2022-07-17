# CLox
Another implementation of ByteCode Interpreter for Lox in C.

## Introduction
CLox is an implementation of the programming language Lox in C. Currently it uses naive single-pass compiler, and only runtime optimization is performed. In future it is planned to have a multi-pass compiler with AST and compiler optimization. The initial version of CLox has only features already present in the original Lox reference implementation, but subsequent versions will continue to add new features to make it a powerful language. This is an experiment on the design and implementation of language features, before I start to implement the backend of my own programming language Mysidia. Stay tuned. 

The original version of Lox programming language can be found at Bob Nystrom's repository:
https://github.com/munificent/craftinginterpreters

## Features
- Scanner, Parser and Single-Pass Compiler
- Stacked based bytecote VM with the basic Op Code support
- Unary and Binary Expression/Operators
- If and Else Statement
- Switch Statement
- While and For Loop
- Functions and Closures
- Classes, Objects and Methods
- Inheritance and this/super keywords
- Embeddable VM for CLox in other host applications(since version 1.1)
- `Object` root class for every class in Lox(since version 1.1)
- Framework for creating native functions, methods and classes(since version 1.1).
- Print statement removed, use native function `print` and `println` instead(since version 1.1).
- Customized Runtime configuration for CLox using clox.ini(since version 1.2)
- Separated integer values and floating point values(since version 1.2).

## Roadmap

### CLox v1.1.0
- VM is no longer a global variable, allowing CLox VM to be embedded in other host applications.
- Full fledged Framework for writing Native functions, methods and classes.
- Root class Object which serves as superclass of every class.
- Remove print statement and replace it by print/println native functions.

### CLox v1.2.0(current version)
- Improved object model - Everything is an object, and every object has a class, including nil, true, false, number, string, function, class etc.
- CLox Standard Library for package `lang` and `util`, which contains classes such as Boolean, Number, String, Array, Dictionary, DateTime, etc.
- Allow customized runtime configurations for KtLox at startup with clox.ini
- Split the Number class, which will distinguish between integers and floating numbers.

### CLox v1.3.0
- Array/Dictionary Literals and square bracket notation for array/dictionary access.
- Anonymous Functions and Closure(Lambda expression) with nonlocal returns.
- Replace C style for loop by Kotlin style for-in loop for collection.
- Clox Standard Library improvement: New package `collection` and `io`.

## FAQ

#### What is the purpose of this project? 
CLox is an implementation of Lox language with bytecode VM instead of treewalk interpreter. It is the last experiment on feature implementations, before I will begin writing my own programming language `Mysidia` in C.

#### What will happen to KtLox? 
Nothing, KtLox development is on hold until I can figure out a way to generate JVM bytecode instead of running the interpreter by walking down the AST. Treewalk interpreters are way too slow to be practical without JIT. 