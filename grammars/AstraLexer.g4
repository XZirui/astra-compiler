// $antlr-format alignTrailingComments true, columnLimit 150, maxEmptyLinesToKeep 1, reflowComments false, useTab false
// $antlr-format allowShortRulesOnASingleLine true, allowShortBlocksOnASingleLine true, minEmptyLines 0, alignSemicolons ownLine
// $antlr-format alignColons trailing, singleLineOverrulesHangingColon true, alignLexerCommands true, alignLabels true, alignTrailers true

lexer grammar AstraLexer;

//
// Operators and Punctuation
//

AT              : '@';
DOT             : '.';
COMMA           : ',';
SEMICOLON       : ';';
COLON           : ':';
QUEST           : '?';
LPAREN          : '(';
RPAREN          : ')';
LBRACKET        : '[';
RBRACKET        : ']';
LBRACE          : '{';
RBRACE          : '}';
ADD             : '+';
SUB             : '-';
MULT            : '*';
DIV             : '/';
MOD             : '%';
NOT             : '!';
ASSIGNMENT      : '=';
ADD_ASSIGNMENT  : '+=';
SUB_ASSIGNMENT  : '-=';
MULT_ASSIGNMENT : '*=';
DIV_ASSIGNMENT  : '/=';
MOD_ASSIGNMENT  : '%=';
ARROW           : '->';
DOUBLE_COLON    : '::';
ELVIS           : '?:';
QUEST_DOT       : '?.';

EQ  : '==';
NEQ : '!=';
LE  : '<=';
GE  : '>=';
LT  : '<';
GT  : '>';

CONJ : '&&';
DISJ : '||';

BIT_AND : '&';
BIT_OR  : '|';
BIT_XOR : '^';
BIT_NOT : '~';
LSHIFT  : '<<';
// No `RSHIFT` rule on purpose: `>>` must lex as two `GT` tokens so that
// nested type arguments like `foo<Bar<Baz>>()` parse correctly.
BIT_AND_ASSIGNMENT : '&=';
BIT_OR_ASSIGNMENT  : '|=';
BIT_XOR_ASSIGNMENT : '^=';
LSHIFT_ASSIGNMENT  : '<<=';
RSHIFT_ASSIGNMENT  : '>>=';

// TODO: More

//
// Keywords
//

IMPORT   : 'import';
VAR      : 'var';
VAL      : 'val';
IF       : 'if';
ELSE     : 'else';
FOR      : 'for';
WHILE    : 'while';
DO       : 'do';
BREAK    : 'break';
CONTINUE : 'continue';
RETURN   : 'return';
FUN      : 'fun';
THROW    : 'throw';
CLASS    : 'class';
THIS     : 'this';

//
// Built-in Types
//

VOID   : 'void';
BOOL   : 'bool';
INT    : 'int';
LONG   : 'long';
FLOAT  : 'float';
DOUBLE : 'double';
CHAR   : 'char';
STRING : 'string';

//
// Modifiers
//

PUBLIC    : 'public';
PRIVATE   : 'private';
PROTECTED : 'protected';

//
// Operators
//

IN : 'in';
IS : 'is';
AS : 'as';

//
// Logical Literals
//

BOOLEAN_LITERAL: 'true' | 'false';

NULL_LITERAL: 'null';

//
// Integer Literals
//

INTEGER_LITERAL:
    DEC_INTEGER_LITERAL
    | HEX_INTEGER_LITERAL
    | OCT_INTEGER_LITERAL
    | BIN_INTEGER_LITERAL
;

//
// Floating Point Literals
//

FLOAT_LITERAL: DOUBLE_LITERAL [fF] | DEC_INTEGER_LITERAL [fF];

// Leading zeros are rejected, like in `DEC_INTEGER_LITERAL`: `01.5` must
// lex as `0` + `1.5` (a syntax error), not as a single token.
DOUBLE_LITERAL
    : DEC_DIGIT_NO_ZERO DEC_DIGIT* (DOT DEC_DIGIT+ DOUBLE_EXPONENT? | DOUBLE_EXPONENT)
    | '0' DOT DEC_DIGIT+ DOUBLE_EXPONENT?
    ;

fragment DOUBLE_EXPONENT: [eE] [+-]? DEC_DIGIT+;

// TODO: Separator
DEC_INTEGER_LITERAL: DEC_DIGIT_NO_ZERO DEC_DIGIT* | '0';

fragment DEC_DIGIT         : [0-9];
fragment DEC_DIGIT_NO_ZERO : [1-9];

// TODO: Separator
HEX_INTEGER_LITERAL: '0' [xX] HEX_DIGIT+;

fragment HEX_DIGIT: [0-9a-fA-F];

OCT_INTEGER_LITERAL: '0' [oO] OCT_DIGIT+;

fragment OCT_DIGIT: [0-7];

BIN_INTEGER_LITERAL: '0' [bB] BIN_DIGIT+;

fragment BIN_DIGIT: [01];

//
// String and Character Literals
//

STRING_LITERAL: '"' (UNICODE_ESCAPE | ESCAPE_SEQUENCE | ~["\\\r\n])* '"';

CHAR_LITERAL: '\'' (UNICODE_ESCAPE | ESCAPE_SEQUENCE | ~['\\\r\n]) '\'';

fragment ESCAPE_SEQUENCE: '\\' ~[\r\n];

fragment UNICODE_ESCAPE: '\\u' HEX_DIGIT HEX_DIGIT HEX_DIGIT HEX_DIGIT;

// TODO Raw string literals are not implemented yet. Recognizing the opening
// delimiter turns a cascade of string tokens into one clear parser error.
RAW_STRING_START: '"""';

//
// Identifiers
//

IDENTIFIER: [a-zA-Z_\u4E00-\u9FFF] [a-zA-Z0-9_\u4E00-\u9FFF]*;

//
// Whitespace and comments
//

WS: [ \t\r\n\u000C]+ -> skip;

COMMENT      : '/*' .*? '*/' -> channel(HIDDEN);
LINE_COMMENT : '//' ~[\r\n]* -> channel(HIDDEN);
