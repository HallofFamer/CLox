parser grammar Lox;
options { tokenVocab = LoxLexer; }

program: declaration* EOF;
declaration: classDecl | funDecl | namespaceDecl | traitDecl | varDecl | statement;
classDecl: CLASS IDENTIFIER (LT IDENTIFIER)? (WITH parameters)? LBRACE CLASS? function* RBRACE;
funDecl: FUN function;
namespaceDecl: NAMESPACE IDENTIFIER (DOT IDENTIFIER)*;
traitDecl: TRAIT IDENTIFIER (WITH parameters);
varDecl: (VAL | VAR) IDENTIFIER (EQ expression)? SEMICOLON;

statement: awaitStmt | breakStmt | continueStmt | exprStmt | forStmt | ifStmt | requireStmt | returnStmt | whileStmt | yieldStmt | block;
awaitStmt: AWAIT expression SEMICOLON;
breakStmt: BREAK SEMICOLON;
continueStmt: CONTINUE SEMICOLON;
exprStmt: expression SEMICOLON;
forStmt: FOR LPAREN (varDecl | exprStmt)? SEMICOLON expression? SEMICOLON expression? RPAREN statement;
ifStmt: IF LPAREN expression RPAREN statement (ELSE statement)?;
requireStmt: REQUIRE STRING SEMICOLON;
returnStmt: RETURN expression? SEMICOLON;
whileStmt: WHILE LPAREN expression RPAREN statement;
yieldStmt: YIELD expression SEMICOLON;
block: LBRACE declaration* RBRACE;

expression: assignment;
assignment: ((call DOT)? IDENTIFIER EQ assignment) | logicOr;
logicOr: (logicAND OR logicAND)*;
logicAND: (equality AND equality)*;
equality: (comparison (BangEQ | EQEQ) comparison)*;
comparison: (term (LT | LTEQ | GT | GTEQ) term)*;
term: (factor (PLUS | MINUS) factor)*;
factor: (unary (STAR | SLASH | MODULUS) unary)*;
unary: ((BANG | MINUS) unary) | call;
call: primary (LPAREN arguments? RPAREN | LSQUARE expression RSQUARE | DOT IDENTIFIER)*;
primary: 'nil' | 'true' | 'false' | INT | FLOAT | STRING | IDENTIFIER | (LPAREN expression RPAREN) | (LSQUARE arguments? RSQUARE) | (FUN functionBody) | (SUPER DOT IDENTIFIER);

function: ASYNC? IDENTIFIER functionBody;
functionBody: LPAREN parameters? RPAREN block;
parameters: IDENTIFIER (COMMA IDENTIFIER)*;
arguments: expression (COMMA expression)*;