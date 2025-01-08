# Design
This document explains the design decisions behind the implemented features of CLox. 

## Why everything is an object? 
Besides the fact that Lox is an OO language, having everything as an object adds a level of consistency. In lesser OO languages such as C++ and Java, programmers have to be aware of the difference between objects and primitive values, and some features work only for objects or primitive values, while this is unnecessary in better OO languages such as Smalltalk and Ruby.

## What are non-local returns and how are they useful? 
CLox's lambda expressions use non-local returns, this allows return from the lambda's enclosing function/method instead of just the lambda itself. Non-local return is a valuable feature that allows exiting a function/method that cannot be done otherwise, a feature I've missed a lot when using Javascript's anonymous/arrow functions.

## Why choose class methods over static methods?
Although they seem identical at first glance, there is a subtle difference between class/static methods. Class methods are actually methods defined on the metaclass, while static methods are just namespaced functions. Class methods use dynamic dispatch and can be polymorphic, they are much more powerful than static methods, and fit better with Lox's object model.

## How are the implementation of Shape and Inline Cache improve performance?
CLox's object fields are implemented as hashtables, and looking up an instance variable has performance hit. With the shape and inline cache, it is possible to access instance variable via index/offset, which is faster than hash table lookup. Unfortunately without JIT, CLox cannot make best use of the inline cache, but the performance improvement is still noticeable.

## Why async/await instead of fiber or green thread?
I am well aware of the controversy surrounding async/await as well as its shortcomings. However, implementing async/await is a valuable learning experience(you need knowledge of generator and promise to pull it off), especially considering how poorly this topic is covered in most compiler/interpreter books. 

## Why type annotations only work for function/method parameter/return types?
As the way it stands now, CLox does not have field/property declaration so it is not possible to assign types to instance variables. For local variables, I am firmly against explicit type declarations, and prefer type inference all the time. This leaves function/method declaration to be the only viable spot where type annotations may be specified.