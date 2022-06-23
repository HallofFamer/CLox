# CLox
Another implementation of ByteCode Interpreter for Lox in C.

## Introduction
CLox is an implementation of the programming language Lox in C. Currently it uses naive single-pass compiler, and only runtime optimization is performed. In future it is planned to have a multi-pass compiler with AST and compiler optimization. The initial version of CLox has only features already present in the original Lox reference implementation, but subsequent versions will continue to add new features to make it a powerful language. This is an experiment on the design and implementation of language features, before I start to implement the backend of my own programming language Mysidia. Stay tuned. 

The original version of Lox programming language can be found at Bob Nystrom's repository:
https://github.com/munificent/craftinginterpreters

## Features
- Scanner, Parser and Single-Pass Compiler
- Stacked based VM with the basic Op Code support
- Unary and Binary Expression/Operators
- If and Else Statement
- While and For Loop
- Switch Statement(challenge from the book)
- Scope and Environment
- Functions and Closures
- Classes, Objects and Methods
- Inheritance and this/super keywords

## Roadmap

### CLox v1.1.0
- Improved object model - Everything is an object, and every object has a class, including nil, true, false, number, string, etc.
- Framework for writing Native functions and classes.
- Root class Object which serves as superclass of every class.
- Remove print statement and replace it by print/println native functions.
