parser grammar Lox;
options { tokenVocab = LoxLexer; }

program: declaration* EOF;
declaration: classDecl | funDecl | namespaceDecl | traitDecl | varDecl | statement;
classDecl: CLASS IDENTIFIER classBody;
funDecl: FUN function;
namespaceDecl: NAMESPACE IDENTIFIER (DOT IDENTIFIER)*;
traitDecl: TRAIT IDENTIFIER traitBody;
varDecl: (VAL | VAR) IDENTIFIER (EQ expression)? SEMICOLON;

statement: awaitStmt | breakStmt | caseStmt | continueStmt | exprStmt | forStmt | ifStmt | requireStmt | returnStmt | switchStmt | usingStmt | whileStmt | yieldStmt | block;
awaitStmt: AWAIT expression SEMICOLON;
breakStmt: BREAK SEMICOLON;
caseStmt: CASE expression COLON statement;
catchStmt: CATCH LPAREN IDENTIFIER IDENTIFIER? RPAREN statement
continueStmt: CONTINUE SEMICOLON;
exprStmt: expression SEMICOLON;
forStmt: FOR LPAREN (varDecl | exprStmt)? SEMICOLON expression? SEMICOLON expression? RPAREN statement;
ifStmt: IF LPAREN expression RPAREN statement (ELSE statement)?;
requireStmt: REQUIRE expression SEMICOLON;
returnStmt: RETURN expression? SEMICOLON;
switchStmt: SWITCH caseStmt* (DEFAULT COLON statement)? 
tryStmt: TRY statement catchStmt (FINALLY statement)?
usingStmt: USING IDENTIFIER (DOT IDENTIFIER)*;
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
factor: (unary (STAR | SLASH | MODULO) unary)*;
unary: ((BANG | MINUS) unary) | call;
call: primary (LPAREN arguments? RPAREN | LSQUARE expression QUESTION? RSQUARE | DOT IDENTIFIER)*;
primary: 'nil' | 'true' | 'false' | INT | FLOAT | STRING | IDENTIFIER | (LPAREN expression RPAREN) | (LSQUARE arguments? RSQUARE) | (expression DOTDOT expression) | (CLASS classBody) | (FUN functionBody) | (SUPER DOT IDENTIFIER) | (TRAIT traitBody);

classBody: (LT IDENTIFIER)? (WITH parameters)? LBRACE function* RBRACE;
function: ASYNC? CLASS? IDENTIFIER? IDENTIFIER functionBody;
functionBody: LPAREN parameters? RPAREN block;
traitBody: (WITH parameters)? LBRACE CLASS? function* RBRACE;
parameters: (VAR? IDENTIFIER? IDENTIFIER (VAR? COMMA IDENTIFIER IDENTIFIER)*) | (DOTDOT IDENTIFIER IDENTIFIER);
arguments: expression (COMMA expression)*;