lexer grammar Lox;

SEPARATOR: [ \t\r\n];
KEYWORD: AND | AS | ASYNC | AWAIT | BREAK | CATCH | CLASS | CONTINUE | DEFAULT | ELSE | FINALLY | FOR | FUN | IF | OR | RETURN | SUPER | THROW| TRAIT | TRY | VAL | VAR | WHILE | WITH | YIELD;
AND: 'and';
AS: 'as';
ASYNC: 'async';
AWAIT: 'await';
BREAK: 'break';
CATCH: 'catch';
CLASS: 'class';
DEFAULT: 'default';
ELSE: 'else';
FINALLY: 'finally';
FOR: 'for';
FUN: 'fun';
IF: 'if';
NAMESPACE: 'namespace';
OR: 'or';
REQUIRE: 'require';
RETURN: 'return';
SUPER: 'super';
SWITCH: 'switch';
THROW: 'throw';
TRAIT: 'trait';
TRY: 'try';
USING: 'using';
VAL: 'val';
VAR: 'var';
WHILE: 'while';
WITH: 'with';
YIELD: 'yield';
DOT: '.';
COMMA: ',';
QUESTION: '?';
SEMICOLON: ';';
EQ: '=';
BANG: '!';
PIPE: '|';
LBRACE: '{';
RBRACE: '}';
LSQUARE: '[';
RSQUARE: ']';
LPAREN: '(';
RPAREN: ')';
EQEQ: '==';
BangEQ: '!=';
LT: '<';
LTEQ: '<=';
GT: '>';
GTEQ: '>=';
PLUS: '+';
MINUS: '-';
STAR: '*';
SLASH: '/';
MODULUS: '%';

INT: DIGIT+;
FLOAT: INT(DOT INT);
NUMBER: INT | FLOAT;
STRING: '\'" (.)*? "\'';
IDENTIFIER: ALPHA (ALPHA | DIGIT)*;
ALPHA: [a-zA-Z_]+;
DIGIT: [0-9];