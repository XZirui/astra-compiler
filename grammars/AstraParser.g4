// $antlr-format alignTrailingComments true, columnLimit 150, minEmptyLines 1, maxEmptyLinesToKeep 1, reflowComments false, useTab false
// $antlr-format allowShortRulesOnASingleLine false, allowShortBlocksOnASingleLine true, alignSemicolons hanging, alignColons hanging

parser grammar AstraParser;

options {
    tokenVocab = AstraLexer;
}

file
    : topLevelObject* EOF
    ;

// TODO: Adjust top level objects
topLevelObject
    : declaration
    ;

declaration
    : visibilityModifier? (functionDecl | variableDecl SEMICOLON | classDecl)
    ;

visibilityModifier
    : PUBLIC | PRIVATE | PROTECTED
    ;

functionDecl
    : FUN IDENTIFIER LPAREN (parameter (COMMA parameter)*)? RPAREN ARROW type block
    ;

classDecl
    : CLASS IDENTIFIER typeParameters? classBody?
    ;

classBody
    : LBRACE declaration* RBRACE
    ;

parameter
    : IDENTIFIER COLON type (ASSIGNMENT expression)?
    ;

type
    : (parenType | typeRef | builtinType) (LBRACKET expression RBRACKET)*
    | functionType
    ;

parenType
    : LPAREN type RPAREN
    ;

typeRef
    : IDENTIFIER typeArguments?
    ;

functionType
    : FUN LPAREN paramTypeList? RPAREN ARROW type
    ;

paramTypeList
    : type (COMMA type)*
    ;

// TODO value for template
// An empty type argument list `<>` is accepted so it can request the default
// type parameters.
typeArguments
    : LT typeArgument? GT
    ;

typeArgument
    : type (COMMA type)*
    ;

// Type parameter declaration lists (`class Box<T, U = Int>`) declare names
// with optional default types, unlike `typeArguments` which reference
// existing types (`Box<int>`). Value parameters are future work.
typeParameters
    : LT typeParameter (COMMA typeParameter)* GT
    ;

typeParameter
    : IDENTIFIER (ASSIGNMENT type)?
    ;

builtinType
    : VOID
    | BOOL
    | INT
    | LONG
    | FLOAT
    | DOUBLE
    | CHAR
    | STRING
    ;

block
    : LBRACE statement* RBRACE
    ;

statement
    : declStatement
    | assignment SEMICOLON
    | controlStatement
    | exprStmt
    ;

declStatement
    : declaration
    ;

// TODO
assignment
    : postfixUnaryExpr assignmentOperator expression
    ;

assignmentOperator
    : ASSIGNMENT
    | ADD_ASSIGNMENT
    | SUB_ASSIGNMENT
    | MULT_ASSIGNMENT
    | DIV_ASSIGNMENT
    | MOD_ASSIGNMENT
    | BIT_AND_ASSIGNMENT
    | BIT_OR_ASSIGNMENT
    | BIT_XOR_ASSIGNMENT
    | LSHIFT_ASSIGNMENT
    | RSHIFT_ASSIGNMENT
    ;

variableDecls
    : variableDecl (COMMA variableDecl)*
    ;

variableDecl
    : VAR IDENTIFIER ((COLON type (ASSIGNMENT expression)?) | ((COLON type)? ASSIGNMENT expression))
    | VAL IDENTIFIER (COLON type)? ASSIGNMENT expression
    ;

controlStatement
    : forStmt
    | forEachStmt
    | whileStmt
    | doWhileStmt
    | ifStmt
    | tryStatement
    ;

forStmt
    : FOR LPAREN variableDecls? SEMICOLON expression? SEMICOLON forUpdate?
    RPAREN block
    ;

forUpdate
    : assignment | expression
    ;

forEachStmt
    : FOR LPAREN IDENTIFIER IN expression RPAREN block
    ;

whileStmt
    : WHILE LPAREN expression RPAREN block
    ;

doWhileStmt
    : DO block WHILE LPAREN expression RPAREN
    ;

ifStmt
    : IF LPAREN expression RPAREN block (ELSE (ifStmt | block))?
    ;

tryStatement
    : TRY block (catchClause+ (FINALLY block)? | FINALLY block)
    ;

catchClause
    : CATCH LPAREN IDENTIFIER COLON type RPAREN block
    ;

exprStmt
    : expression SEMICOLON
    ;

expression
    : disjunction
    ;

disjunction
    : conjunction (DISJ conjunction)*
    ;

conjunction
    : equality (CONJ equality)*
    ;

equalityOperator
    : EQ
    | NEQ
    ;

equality
    : comparison (equalityOperator comparison)?
    ;

comparisonOperator
    : LT
    | GT
    | LE
    | GE
    ;

// A dangling `>` directly before `(` prefers the generic-call reading (Kotlin
// rule), e.g. `foo<Int>>(x)` / `a < b >> (x)`: the second `>` cannot close the
// type argument list, so the builder reports an error. A single `>` before `(`
// (`a < b > (c)`) is already consumed by the postfix layer's `callSuffix` and
// never reaches this alternative. Keep this alternative first: ambiguous
// inputs resolve to the smallest alternative, so the generic reading wins.
comparison
    : infixExpr typeArguments GT LPAREN valueArguments? RPAREN
    | infixExpr (comparisonOperator infixExpr)*
    ;

inOperator
    : IN
    ;

isOperator
    : IS
    ;

infixExpr
    : elvisExpr (inOperator elvisExpr | isOperator type)*
    ;

elvisOperator
    : ELVIS
    ;

elvisExpr
    : bitwiseOr (elvisOperator bitwiseOr)*
    ;

bitwiseOr
    : bitwiseXor (BIT_OR bitwiseXor)*
    ;

bitwiseXor
    : bitwiseAnd (BIT_XOR bitwiseAnd)*
    ;

bitwiseAnd
    : bitwiseShift (BIT_AND bitwiseShift)*
    ;

bitwiseShiftOperator
    : LSHIFT
    // `>>` lexes as two `GT` tokens (there is no `RSHIFT` token), so
    // nested type arguments like `foo<Bar<Baz>>()` parse correctly. Only a
    // physically adjacent `>>` is a right shift: `a > > b` (whitespace or a
    // comment between the two `>`) is reported as an error by the builder.
    | GT GT
    ;

bitwiseShift
    : addition (bitwiseShiftOperator addition)*
    ;

additionOperator
    : ADD
    | SUB
    ;

addition
    : multiplication (additionOperator multiplication)*
    ;

multiplicationOperator
    : MULT
    | DIV
    | MOD
    ;

multiplication
    : asExpr (multiplicationOperator asExpr)*
    ;

asOperator
    : AS
    | AS QUEST
    ;

asExpr
    : prefixUnaryExpr (asOperator type)?
    ;

// TODO annotation
unaryPrefix
    : prefixUnaryOperator
    ;

prefixUnaryOperator
    : ADD
    | SUB
    | NOT
    | BIT_NOT
    ;

prefixUnaryExpr
    : unaryPrefix* postfixUnaryExpr
    ;

unaryPostfix
    : callSuffix
    | indexingSuffix
    | navigationSuffix
    ;

// Type arguments are only valid directly before the argument list, e.g.
// `foo<Int>()`. A bare `foo<Int>` is a syntax error.
callSuffix
    : typeArguments? LPAREN valueArguments? RPAREN
    ;

valueArguments
    : valueArgument (COMMA valueArgument)*
    ;

valueArgument
    : ((IDENTIFIER ASSIGNMENT) | MULT)? expression
    ;

indexingSuffix
    : LBRACKET expression RBRACKET
    ;

navigationSuffix
    : memberAccessOperator IDENTIFIER
    ;

memberAccessOperator
    : QUEST_DOT
    | DOT
    | DOUBLE_COLON
    ;

postfixUnaryExpr
    : primaryExpr unaryPostfix*
    ;

primaryExpr
    : parenExpr
    | literalConstant
    | IDENTIFIER
//    | functionLiteral
    | collectionLiteral
    | thisLiteral
    | ifExpression
    | jumpExpression
    ;

parenExpr
    : LPAREN expression RPAREN
    ;

literalConstant
    : BOOLEAN_LITERAL
    | INTEGER_LITERAL
    | FLOAT_LITERAL
    | DOUBLE_LITERAL
    | NULL_LITERAL
    | STRING_LITERAL
    | CHAR_LITERAL
    ;


// TODO
functionLiteral
    :
    ;

// TODO
collectionLiteral
    : LBRACKET (expression (COMMA expression)*)? RBRACKET
    ;

thisLiteral
    : THIS
    ;

ifExpression
    : IF LPAREN expression RPAREN expression (ELSE expression)?
    ;

jumpExpression
    : THROW expression
    | RETURN expression?
    | CONTINUE label?
    | BREAK label?
    ;

// TODO attach to statement
label
    : AT IDENTIFIER
    ;
